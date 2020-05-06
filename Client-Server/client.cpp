/*
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 2/8/20
 */
#include "common.h"
#include "FIFOreqchannel.h"


int main(int argc, char *argv[])
{
    int pid = fork();
    if(pid == 0)
    {
        char* args[] = {"./server", NULL};
        execvp(args[0],args);
    }
    else
    {
        int opt;
        int p, e, m;
        double t;
        string filename;
        int buffercapacity = MAX_MESSAGE;

        bool pflag = false;
        bool eflag = false;
        bool fflag = false;
        bool mflag = false;
        bool tflag = false;
        

        while((opt = getopt(argc, argv, "p:t:e:m:f:c")) != -1)
        {
            switch(opt)
            {
                case 'p':
                p = atoi(optarg);
                pflag = true;
                break;

                case 't':
                t = atoi(optarg);
                tflag = true;
                break;

                case 'e':
                e = atoi(optarg);
                eflag = true;
                break;

                case 'm':
                buffercapacity = atoi(optarg);
                std::cout << buffercapacity << endl;
                mflag = true;
                break;

                case 'f':
                filename = optarg;
                fflag = true;
                break; 
            }
        }
        int n = 100;
        int people = 15;

        srand(time_t(NULL));

        FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);

        if(pflag)
        {
            string filename1 = "received/x1.csv";

            ofstream file;
            file.open(filename1);

            //User Requested Data Point
            if(tflag && eflag)
            {
                std::cout << tflag << " " << eflag << endl;
                datamsg d = datamsg(p, t, e);

                char* buffer = new char[sizeof(d)];
                *(datamsg*)buffer = d;
                chan.cwrite(buffer, sizeof(datamsg));
                chan.cread(buffer, sizeof(datamsg));
                double data = *(double*)buffer;

                std::cout << "User Requested Patient: " << data << endl;
                
                //EXIT THE CHANNEL
                MESSAGE_TYPE m2 = QUIT_MSG;
                chan.cwrite (&m2, sizeof (MESSAGE_TYPE));
            }

            //COPYING ALL P.CSV INTO XP.CSV
            if(!tflag && !eflag)
            {
                struct timeval start, end;
                gettimeofday(&start, NULL);

                double i = 0;
                
                while(i < 59.996)
                {
                    file << i << ",";

                    datamsg d1 = datamsg(p,i,1);
                    datamsg d2 = datamsg(p,i,2);

                    char* buffer1 = new char[sizeof(d1)];
                    *(datamsg*)buffer1 = d1;
                    chan.cwrite(buffer1,sizeof(datamsg));
                    chan.cread(buffer1, sizeof(datamsg));
                    double data1 = *(double*)buffer1;

                    file << data1 << ",";

                    char* buffer2 = new char[sizeof(d1)];
                    *(datamsg*)buffer2 = d2;
                    chan.cwrite(buffer2,sizeof(datamsg));
                    chan.cread(buffer2, sizeof(datamsg));
                    double data2 = *(double*)buffer2;

                    file << data2 << "\n";

                    i = i + .004;
                }
                std::cout << "Successfully transferred all data points of Person 1" << endl;

                gettimeofday(&end, NULL);

                double timeTaken;

                timeTaken = (end.tv_sec - start.tv_sec) * 1e6;
                timeTaken = (timeTaken + (end.tv_usec - start.tv_usec)) * 1e-6;

                std::cout << "Time taken to transfer all 1.csv datapoints: " << fixed << timeTaken << setprecision(6);
                std::cout << " sec" << endl;
            }

            file.close();
        }


        //REQUESTING FILES
        if(fflag)
        {
            filemsg f1 = filemsg(0,0);
            // -----------------------------------------------------------------------//
            char* buffer3 = new char[sizeof(filemsg) + sizeof(filename) + 1];
            char* recbuf = new char[buffercapacity];
            *(filemsg*)buffer3 = f1;
            //Copy the file name to destination buffer3 + sizeof(filemsg)
            strcpy(buffer3 + sizeof(filemsg), filename.c_str());
            chan.cwrite(buffer3, sizeof(filemsg) + sizeof(filename) + 1);
            chan.cread(recbuf, buffercapacity);
            __int64_t fileLength = *(__int64_t*)recbuf;
            std::cout << fileLength << endl;

            //read data
            //start timer
            struct timeval start2, end2;
            gettimeofday(&start2, NULL);
            //sync the I/O of C and C++
            ios_base::sync_with_stdio(false);

             FILE* copyFile;

             char path[50] = "received/";
             strcat(path, filename.c_str());
            
             copyFile = fopen(path, "wb");

            int offset = 0;
            int length = buffercapacity;
            while (offset < fileLength)
            {
                if(fileLength - offset < buffercapacity)
                {
                    length = fileLength - offset;
                }
                filemsg f2 = filemsg(offset, length);
                char* bufferf = new char[sizeof(filemsg) + sizeof(filename) + 1];
                char* recvbuf = new char[buffercapacity];
                *(filemsg*)bufferf = f2;
                strcpy(bufferf + sizeof(filemsg), filename.c_str());
                //*(char*)(bufferf + sizeof(filemsg) + sizeof(filename)) = '\0';
                chan.cwrite(bufferf, sizeof(filemsg) + sizeof(filename) + 1);
                chan.cread(recvbuf, length);
               
                fwrite(recvbuf, 1, length, copyFile);
                //cout << "BUFFER: " << bufferf << endl;
                //cout << "COUNT: " << count <<" OFFSET/LENGTH: "<< offset << " , " << length << endl;
                //increase the bytes offset
                offset += buffercapacity;
            }


            fclose(copyFile);

            gettimeofday(&end2, NULL);

            double timeTaken2;

            timeTaken2 = (end2.tv_sec - start2.tv_sec) * 1e6;
            timeTaken2 = (timeTaken2 + (end2.tv_usec - start2.tv_usec)) * 1e-6;

            std::cout << "Time Taken to copy file: " << timeTaken2 << fixed << setprecision(6) << endl;

            //QUIT MESSAGE
            MESSAGE_TYPE m1 = QUIT_MSG;
            chan.cwrite (&m1, sizeof (MESSAGE_TYPE));
        }

        if(!eflag && !mflag && !fflag && !pflag)
        {
            newchannelmsg ncm = newchannelmsg();
            char* ncmbuffer = new char[buffercapacity];
            *(newchannelmsg*)ncmbuffer = ncm;
            chan.cwrite(ncmbuffer, sizeof(newchannelmsg));
            FIFORequestChannel chan2 ("data1_", FIFORequestChannel::CLIENT_SIDE);

            //using new channel
            std::cout << "Testing Data Points on New Channel" << endl;
            datamsg d3 = datamsg(2, .008, 1);
            char* buffertest = new char[sizeof(d3)];
            *(datamsg*)buffertest = d3;
            chan2.cwrite(buffertest, sizeof(datamsg));
            chan2.cread(buffertest, sizeof(datamsg));
            double dataTest = *(double*)buffertest;
            std::cout << dataTest << endl;

            std::cout << "Testing Data Points on New Channel" << endl;
            datamsg d4 = datamsg(1, .016, 2);
            char* bufferTest2 = new char[sizeof(d4)];
            *(datamsg*)bufferTest2 = d4;
            chan2.cwrite(bufferTest2, sizeof(datamsg));
            chan2.cread(bufferTest2, sizeof(datamsg));
            double dataTest2 = *(double*)bufferTest2;
            std::cout << dataTest2 << endl;




            //QUIT MESSAGE
            MESSAGE_TYPE m3 = QUIT_MSG;
            chan.cwrite (&m3, sizeof (MESSAGE_TYPE));
        }
        quitmsg q = quitmsg();
        char* quitbuffer = new char[sizeof(q)];
        *(quitmsg*)quitbuffer = q;
        chan.cwrite(quitbuffer, sizeof(quitmsg));
    }
    return 0;
}
