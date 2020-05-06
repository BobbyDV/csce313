#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
#include <thread>
using namespace std;

void timediff(struct timeval& start, struct timeval& end){

}

FIFORequestChannel* create_new_channel (FIFORequestChannel* mainchan){
    char name [1024];
    MESSAGE_TYPE m = NEWCHANNEL_MSG;
    mainchan->cwrite(&m, sizeof(m));
    mainchan->cread(name, 1024);
    FIFORequestChannel* newchan = new FIFORequestChannel(name, FIFORequestChannel::CLIENT_SIDE);
    return newchan;
}


void * patient_thread_function(int n, int pno, BoundedBuffer* rb, bool pflag){
    /* What will the patient threads do? */
    if(pflag){
        datamsg d (pno, 0.0, 1);
        double resp = 0;
        for(int i = 0; i < n; i++){
            rb->push((char *)&d, sizeof(d));
            d.seconds += 0.004;
        }
    }
}

void file_thread_function(string fname, BoundedBuffer* rb, FIFORequestChannel* chan, int mb, bool fflag){
    if(fflag){
        //1. create the file
        string recvfname = "recv/" + fname;

        //1.1 make it as long as the original length
        char buf[1024];
        //get the length of the original file
        filemsg f (0,0);
        //copy size of the file to the buffer and copy the filename to the end
        memcpy(buf, &f, sizeof(f));
        strcpy(buf + sizeof(f), fname.c_str());
        //write to the server whats in the buf, with a size of the file + filename + terminating char
        chan->cwrite(buf, sizeof(f) + fname.size() + 1);
        //get the length of the original file
        __int64_t fileLength;
        chan->cread(&fileLength, sizeof(fileLength));
        //create the (empty) copy file
        FILE* fp = fopen(recvfname.c_str(), "w");
        fseek(fp, fileLength, SEEK_SET);
        fclose(fp);

        //2. generate all the file messages
        filemsg* fm = (filemsg *)buf;
        __int64_t remlen = fileLength;

        while(remlen > 0){
            fm->length = min(remlen, (__int64_t) mb);
            rb->push(buf, sizeof(filemsg) + fname.size() + 1);
            fm->offset += fm->length;
            remlen -= fm->length;
        }
    }

}

void *worker_thread_function(FIFORequestChannel* chan, BoundedBuffer* rb, HistogramCollection* hc, int mb){
    char buf[1024];
    double resp;

    char recvBuf[mb];
    while(true){
        rb->pop(buf, 1024);
        MESSAGE_TYPE* m = (MESSAGE_TYPE *)buf;

        if(*m == DATA_MSG){
            chan->cwrite(buf, sizeof(datamsg));
            chan->cread(&resp, sizeof(double));
            hc->update(((datamsg *)buf)->person, resp);
        }
        else if(*m == QUIT_MSG){
            chan->cwrite(m, sizeof(MESSAGE_TYPE));
            delete chan;
            break;
        }else if(*m == FILE_MSG){
            filemsg* fm = (filemsg *)buf;
            string fname = (char *)(fm + 1);
            int size = sizeof(filemsg) + fname.size() + 1;
            chan->cwrite(buf, size);
            chan->cread(recvBuf, mb);

            string recvfname = "recv/" + fname;

            FILE* fp = fopen(recvfname.c_str(), "r+");
            fseek(fp, fm->offset , SEEK_SET);
            fwrite(recvBuf, 1, fm->length, fp);
            fclose(fp);
        }
    }
}



int main(int argc, char *argv[])
{
    int n = 15000;    //default number of requests per "patient"
    int p = 1;     // number of patients [1,15]
    int w = 200;    //default number of worker threads
    int b = 500; 	// default capacity of the request buffer, you should change this default
	int m = MAX_MESSAGE; 	// default capacity of the message buffer
    string serverm = "-m ";
    char servermArr[10];
    srand(time_t(NULL));
    string fname;
    bool pflag, fflag = false;

    int opt = -1;
    while((opt = getopt(argc, argv, "m:n:b:w:p:f:")) != -1){
        switch(opt){
            case 'm':
                serverm.append(optarg);
                strcpy(servermArr, serverm.c_str());
                break;
            case 'n':
                n = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'p':
                p = atoi(optarg);
                pflag = true;
                break;
            case 'w':
                w = atoi(optarg);
                break;
            case 'f':
                fname = optarg;
                fflag = true;
                break;
        }
    }
    
    
    int pid = fork();
    if (pid == 0){
		// modify this to pass along m
        execl ("server", "server", servermArr, (char *)NULL);
    }
    
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
	HistogramCollection hc;

    // making Histograms and adding to the histogram collection hc
    if(pflag){
        for(int i = 0; i < p; i++){
            Histogram* h = new Histogram(10, -2.0, 2.0);
            hc.add(h);
        }
    }

    // make w worker channels
    FIFORequestChannel* wchans [w];
    for(int i = 0; i < w; i++){
        wchans[i] = create_new_channel(chan);
    }
	
	
    struct timeval start, end;
    gettimeofday (&start, 0);

    /* Start all threads here */
    //Patient threads
    thread patient[p];
    for(int i = 0; i < p; i++){
        patient[i] = thread(patient_thread_function, n, i+1, &request_buffer, pflag);
    }

    //file thread
    thread filethread(file_thread_function, fname, &request_buffer, chan, m, fflag);
    

    //worker threads
    thread workers[w];
    for(int i = 0; i < w; i++){
        workers[i] = thread(worker_thread_function, wchans[i], &request_buffer, &hc, m);
    }
	

	/* Join all threads here */
    //patient threads
    for(int i = 0; i < p; i++){
        patient[i].join();
    }

    //file thread
    filethread.join();

    std::cout << "Patient threads/file thread finished" << endl;

    // send quit message after patient/file thread finishes
    for(int i = 0; i < w; i++){
        MESSAGE_TYPE q = QUIT_MSG;
        request_buffer.push((char *) &q, sizeof(q));
    }

    //worker threads
    for(int i = 0; i < w; i++){
        workers[i].join();
    }
    cout << "Worker threads finished" << endl;

    //delete worker and patient threads
    

    gettimeofday (&end, 0);
    // print the results
	hc.print ();

    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;
    
}
