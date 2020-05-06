#include <iostream>
#include <vector>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ctime>

using namespace std;

//Taken from stackoverflow
//takes in a string and splits it up by the delimiter
vector<string> split(string &input, string delimiter)
{
    vector<string> splitInput;
    size_t pos = 0;
    string token;

    

    while((pos = input.find(delimiter)) != string::npos) // while pos does not equal the maximum size possible
    {
        if(input[0] == 'e') //if the command is an echo command
        {
            break;
        }

        token = input.substr(0,pos - 1); //the commands we process later
        splitInput.push_back(token);
        if(delimiter == "|") //splitting will leave a space after the pipe if +1 is not there
        {                                       
            input.erase(0, pos + delimiter.length() + 1);
        }
        else //the +1 above cuts off the first letter of strings, so different command needed
            input.erase(0, pos + delimiter.length());
    }

    splitInput.push_back(input);

    return splitInput;
}

//changes stdout to file passed in
void outputIO(string file)
{
    int fd = open(file.c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    dup2(fd, 1);
}

//changes stdin to file passed in
void inputIO(string file)
{
    int fd = open(file.c_str(), O_RDONLY);
    dup2(fd, 0);
}

//split the command by spaces to find and execute all the arguments
void execute(string input)
{
    int found;
    int index = 0;
    char* parsed; //our parsed input
    char *command[100]; //the array of the parsed input

    //ECHO operation
    if(input.find("echo") != string::npos)
    {
        parsed = strtok((char*)input.c_str(), " ");

        if(int special = input.find("-e") != string::npos) //if there is an -e flag
        {                                                  //parse one more time by space
            command[index] = parsed;            
            index ++;

            parsed = strtok(NULL, " ");
            
        }

        while(parsed != NULL)
        {
            command[index] = parsed;
            index++;

            parsed = strtok(NULL, "\"\'");
        }

        command[index] = NULL;

        execvp(command[0],command);
    }

    //if output requires outputIO operation
    if((found = input.find(">")) != string::npos)
    {
        outputIO(input.substr(found + 2)); //filename substring
        input.erase(input.begin() + found - 1, input.end()); //get rid of everything after >
    }

    //if input requires inputIO operation
    if((found = input.find("<")) != string::npos)
    {
        inputIO(input.substr(found + 2)); //filename substring
        input.erase(input.begin() + found - 1, input.end()); //get rid of everything after <
    }

    //split up the awk command by quotes
    if(input.find("awk") != string::npos)
    {
        parsed = strtok((char*)input.c_str(), " ");

        while(parsed != NULL) //while we still have tokens
        {
            command[index] = parsed;
            index++;

            parsed = strtok(NULL, "\'"); //keep searching for tokens split by '\''
        }
    }

    //splits up any other command by spaces
    else
    {
        parsed = strtok((char*)input.c_str(), " ");
    
        while(parsed != NULL) //while we still have tokens
        {
            command[index] = parsed;
            index++;

            parsed = strtok(NULL, " "); //keep searching for tokens split by ' '
        }
    }

    command[index] = NULL; //setting the last element of the array to NULL to terminate array

    execvp(command[0], command);
}

int main()
{
    bool background;
    int status;
    int amp;
    string date;
    time_t now;
    char* prompt[100];
    vector<int> processes;
    int stdoutBackUp = dup(1);
    int stdinBackUp = dup(0);
    while(true)
    {
        now = time(0);
        date = ctime(&now);
        background = false;
        dup2(stdoutBackUp, 1);
        dup2(stdinBackUp, 0);
        string inputline;
        cout << date << "/BobbyV$ ";
        inputline = readline(NULL);
        add_history(inputline.c_str());

        //handles background process
        if((amp = inputline.find("&")) != string::npos)
        {
            inputline.erase(inputline.begin() + amp - 1, inputline.end());
            background = true;
        }

        vector<string> c = split(inputline, "|"); //split the input by the pipes
        
        if(inputline == string("exit")) //exit the shell
        {
            cout << "End of shell" << endl;
            exit(0);
        }

        //prints out the date

        if(inputline == string("jobs")) //prints out all processes currently running
        {
            for(int i = 0; i < processes.size(); i++)
            {
                cout << "PID: " << processes[i] << endl;
            }

            continue; //skip fork
        }

        //handles the CD command
        if(inputline.find("cd") != string::npos)
        {
            if(inputline.find("-") != string::npos)
            {
                chdir("..");
            }
            else
            {
                vector<string> path = split(inputline, " ");
                if(chdir((char *)path[1].c_str()) < 0)
                {
                    perror(path[1].c_str());
                }
            }

            //skip the fork
            continue;
        }

        for(int i = 0; i < c.size(); i++)
        {
            //cout << c[i] << endl;
            //set up pipe
            int fd[2];
            pipe(fd);
            int child_pid = fork();
            if(!child_pid) //in the child process
            {
                //1.redirect the output to the next level unless this is the last level
                if(i < c.size() - 1)
                {
                    dup2(fd[1], 1); //redirect STDOUT to fd[1], so that it can write to the other side
                }

                //2. execute the command at this level
                execute(c[i]);
            }
            else //parent process
            {
                if(background) //if the process is a background process
                {
                    processes.push_back(child_pid); //push it into the process vector
                    waitpid(child_pid, 0, WNOHANG);
                }

                if(i == c.size() - 1 && !background) //wait only for the last child
                {
                    waitpid(child_pid, 0, 0);

                }
                dup2(fd[0],0); //now redirect the input for the next loop iteration
                
                close(fd[1]); //fd[1] MUST be closed, otherwise the next level will fail
            }

            //goes through processes and reaps any background processes
            for(int i = 0; i < processes.size(); i++)
            {
                status = waitpid(processes[i], 0, WNOHANG);

                if(status > 0) //process is finished
                {
                    processes.erase(processes.begin() + i);
                }
            }
        }
        
        
        
        
        
        
        
        
        
        

        //I/O Redirection
        // int pid2 = fork();
        // if(pid2 != 0) //child process
        // {
        //     int fd = open("foo.txt", O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        //     dup2(fd,1); //overwriting stdout with the new file
        //     execlp("ls","ls","-l","-a",NULL);
        // }
        // else
        // {
        //     wait(0);
        // }


        
        
    }
}