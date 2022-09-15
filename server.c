#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/signal.h>
#include <fcntl.h>

char reply[4096]; //variable to store reply
int client = -1, pid, sd = -1, dataConnection = -1, dataSocket = -1, flag = 0, renameFlag = 0, gotClient = 0, clientPid, childCount = 0, childPID[1000]; //client: to store control port, pid: to store processid of child, sd: to store control socket, dataConnection: to store data port, dataSocket: to store data socket, flag: to handle USER login, renameFlag: to handle rename functionality, gotClient: to check whether client is there or not just for synchronization, clientPID: to store pid of client, childCount: to track how many clients are connected, childPID: to store pid of every child

//========================== function to handle signal ==========================

void signal_handler(int signo){
    int status;
    if(sd > 0 && pid != 0){ //checking it is in parent or not
        for(int i = 0; i < childCount; i++){ //if in parent loop for every child
            kill(childPID[i], SIGINT); //sending SIGINT signal to child
            wait(&status); //waiting for status 
        }
        close(sd); //closing socket in parent
        exit(0); //exiting parent after death of every child
    }
    else if(pid == 0) { //if it is child
        memset(reply, 0, sizeof(reply));
        strcat(reply, "221 Service closing control connection.");
        write(client, reply, strlen(reply)); //sending client message that server is closing connection
        kill(clientPid, SIGINT); //sending SIGINT signal to client 
        sleep(2); 
        if(dataConnection > 0){ //checking if data port is open or not
            close(dataConnection); //if open closing data port
            close(dataSocket); //closing data socket
        }
        close(client); //closing control port
        exit(0); //exiting from child
    }
    
}

//========================== function to change directory ==========================

int change_directory(char *dir){
    return chdir(dir); //changing directory and returning status of action
}

//========================== function to make directory ==========================

int make_directory(char *dir){
    return mkdir(dir, 0777); //making directory and returning status of action
}

//========================== function to open data port ==========================

int open_port(char *newPort){
    struct sockaddr_in dataAdd;

    if((dataSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){ //creating socket for data port
        printf("Error: Cannot create data socket\n"); //if not created printing error
        return -1; //return -1
    }
    
    dataAdd.sin_family = AF_INET; //using IPv4 protocol 
    dataAdd.sin_addr.s_addr = htonl(INADDR_ANY); //setting address
    dataAdd.sin_port = htons(atoi(newPort)); //setting new port for data transfer provided by client
    
    if(bind(dataSocket, (struct sockaddr*)&dataAdd, sizeof(dataAdd)) >= 0){ //binding the data socket
        if(listen(dataSocket, 5) == 0){ //listening through data socket
            while(1){ 
                if((dataConnection = accept(dataSocket, (struct sockaddr *)NULL, NULL)) > 0){ //accepting data port connection from client
                    return 0; //if accepted return 0
                }
            }
        }
    }
    return -1; //if anything fails return -1
}

//========================== function to handle PORT request ==========================

void port(char *request){
    if(dataConnection < 0){ //checking data port is open or not
        char *token = strtok(request, " "); //if open splitting request by space
        if((token = strtok(NULL, " ")) != NULL){ //checking for argument 
            if(open_port(token) != -1){ //if argument is provided calling open_port function
                memset(reply, 0, sizeof(reply));
                strcat(reply, "225 Data connection open; no transfer in progress.");
                write(client, reply, strlen(reply)); //if connection established successfully sending response to client
            }
            else{
                memset(reply, 0, sizeof(reply));
                strcat(reply, "425 Can't open data connection.");
                write(client, reply, strlen(reply)); //if connection is not established sending response to client
            }
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "501 Syntax error in parameters or arguments.");
            write(client, reply, strlen(reply)); //if argument is not provided sending response to client
        }
        memset(token, 0, sizeof(token));
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "125 Data connection already open.");
        write(client, reply, strlen(reply)); //if data port is already open sending response to client
    }
}

//========================== function to handle USER request ==========================

void user(){
    memset(reply, 0, sizeof(reply));
    strcat(reply, "230 User logged in, proceed.");
    write(client, reply, strlen(reply)); //sending client response 
}

//========================== function to handle CWD request ==========================

void cwd(char *request){
    char *token = strtok(request, " "); //splitting request by space
    if((token = strtok(NULL, " ")) != NULL){ //checking for argument
        if(change_directory(token) != -1){ //if argument is provided calling change_directory function
            memset(reply, 0, sizeof(reply));
            strcat(reply, "200 directory changed to ");
            strcat(reply, token);
            write(client, reply, strlen(reply)); //if directory changed sending response to client
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "431 No such directory");
            write(client, reply, strlen(reply)); //if directory is not changed sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "501 Syntax error in parameters or arguments.");
        write(client, reply, strlen(reply)); //if argument is not provided sending response to client
    }
}

//========================== function to handle CDUP request ==========================

void cdup(){
    if(change_directory("..") != -1){ //calling change_directory function with argument ".." to go back to parent directory
        memset(reply, 0, sizeof(reply));
        strcat(reply, "200 Changed to the parent");
        write(client, reply, strlen(reply)); //if directory changed sending response to client
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "431 No such directory");
        write(client, reply, strlen(reply)); //if directory is not changed sending response to client
    }
}

//========================== function to handle CDUP request ==========================

void mkd(char *request){
    char *token = strtok(request, " "); //splitting request by space
    if((token = strtok(NULL, " ")) != NULL){ //checking for argument
        if(make_directory(token) != -1){ //if argument is provided calling make_directory function
            memset(reply, 0, sizeof(reply));
            strcat(reply, "257 ");
            strcat(reply, token);
            strcat(reply, " directory created");
            write(client, reply, strlen(reply)); //if directory created sending response to client
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "521 ");
            strcat(reply, token);
            strcat(reply, " directory already exists");
            write(client, reply, strlen(reply)); //if directory is not created sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "501 Syntax error in parameters or arguments.");
        write(client, reply, strlen(reply)); //if argument is not provided sending response to client
    }
}

//========================== function to remove directory ==========================

static int remove_directory(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf){ //this is function to handle nftw call
    return remove(fpath); //removing file or directory and returning status
}

//========================== function to handle RMD request ==========================

void rmd(char *request){
    char *token = strtok(request, " "); //splitting request by space
    if((token = strtok(NULL, " ")) != NULL){ //checking for argument
        if (nftw(token, remove_directory, 20, FTW_DEPTH | FTW_PHYS) != -1){ //calling nftw to traverse through directory
            memset(reply, 0, sizeof(reply));
            strcat(reply, "257 ");
            strcat(reply, token);
            strcat(reply, " directory removed");
            write(client, reply, strlen(reply)); //if directory removed sending response to client
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "521 ");
            strcat(reply, token);
            strcat(reply, " directory not exists");
            write(client, reply, strlen(reply)); //if directory is not removed sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "501 Syntax error in parameters or arguments.");
        write(client, reply, strlen(reply)); //if argument is not provided sending response to client
    }
}

//========================== function to handle DELE request ==========================

void dele(char *request){
    char *token = strtok(request, " "); //splitting request by space
    if((token = strtok(NULL, " ")) != NULL){ //checking for argument
        if (remove(token) != -1){ //if argument is provided calling remove function
            memset(reply, 0, sizeof(reply));
            strcat(reply, "257 ");
            strcat(reply, token);
            strcat(reply, " file removed");
            write(client, reply, strlen(reply)); //if file removed sending response to client
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "521 ");
            strcat(reply, token);
            strcat(reply, " file not exists");
            write(client, reply, strlen(reply)); //if file is not removed sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "501 Syntax error in parameters or arguments.");
        write(client, reply, strlen(reply)); //if argument is not provided sending response to client
    }
}

//========================== function to handle RNFR request ==========================

void rnfr(){
    memset(reply, 0, sizeof(reply));
    strcat(reply, "501 Syntax error in parameters or arguments.");
    write(client, reply, strlen(reply)); //sending response to client that syntax error because RNFR need to followed by RNTO
}

//========================== function to rename file ==========================

int rename_file(char *newFileName, char *oldFileName){
    return rename(oldFileName, newFileName); //renaming file and returning status
}

//========================== function to handle RNFR request ==========================

void rnto(char *request){
    char *newFileName = strtok(request, " "); //splitting request by space
    if((newFileName = strtok(NULL, " ")) != NULL){ //checking for first argument
        char *oldFileName = strtok(NULL, " ");
        if(!strncmp(oldFileName, "RNFR", 4)){ //checking second argument if it is RNFR or not
            if((oldFileName = strtok(NULL, " ")) != NULL){ //if it is checking third argument
                if (rename_file(newFileName, oldFileName) == 0){ //if third argument is provided calling rename_file function
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "250 ");
                    strcat(reply, oldFileName);
                    strcat(reply, " renamed by ");
                    strcat(reply, newFileName);
                    write(client, reply, strlen(reply)); //if file renamed sending response to client
                }
                else{
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "521 ");
                    strcat(reply, oldFileName);
                    strcat(reply, " file not exists");
                    write(client, reply, strlen(reply)); //if file is not renamed sending response to client
                }
            }
            else{
                memset(reply, 0, sizeof(reply));
                strcat(reply, "501 Syntax error in parameters or arguments.");
                write(client, reply, strlen(reply)); //if third argument is not provided sending response to client
            }
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "501 Syntax error in parameters or arguments.");
            write(client, reply, strlen(reply)); //if second argument is not provided sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "501 Syntax error in parameters or arguments.");
        write(client, reply, strlen(reply)); //if first argument is not provided sending response to client
    }
}

//========================== function to handle LIST request ==========================

void list(char *request){
    if(dataConnection > 0){ //checking data port is open or not
        char *token = strtok(request, " "), dirName[1024]; //if open splitting request
        if((token = strtok(NULL, " ")) != NULL){ //checking if argument is provided or not
            strcpy(dirName, token); //if provided setting it as directory name
        }
        else{
            getcwd(dirName, 1024); //if not setting present working directory as directory name
        }
        char list[4096];
        list[0] = '\0';
        
        DIR *dp;
        struct dirent *dirp;
        if((dp = opendir(dirName)) != NULL){ //opening directory
            strcat(list, "125 Data connection already open. transfer starting\n");
            write(client, list, strlen(list)); //sending sucsess response to client
            memset(list, 0, sizeof(list));
            while((dirp = readdir(dp)) != NULL){ //loop to traverese directory
                strcat(list, dirp->d_name); //concatinating list array by name of file or directory visited
                strcat(list, "\n"); //adding new line character
            }
            closedir(dp); //closing directory
            write(dataConnection, list, strlen(list)); //sending list to client by data port 
            memset(reply, 0, sizeof(reply));
            sleep(2);
            strcat(reply, "250 Requested file action okay, completed.");
            write(client, reply, strlen(reply)); //sending response to client
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "550 Requested action not taken.");
            write(client, reply, strlen(reply)); //if directory not opened sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "425 Can't open data connection.");
        write(client, reply, strlen(reply)); //if data port is not open sending response to client
    }
}

//========================== function to handle STAT request ==========================

void stat_function(char *request){
    char *token = strtok(request, " "); //splitting request
    if((token = strtok(NULL, " ")) != NULL){ //checking if argument is provided or not
        char list[4096];
        list[0] = '\0';
        
        DIR *dp;
        struct dirent *dirp;
        if((dp = opendir(token)) != NULL){ //if open opening directory
            strcat(list, "125 Data connection already open. transfer starting\n");
            write(client, list, strlen(list)); //sending sucsess response to client
            memset(list, 0, sizeof(list));
            while((dirp = readdir(dp)) != NULL){ //loop to traverese directory
                strcat(list, dirp->d_name); //concatinating list array by name of file or directory visited
                strcat(list, "\n"); //adding new line character
            }
            closedir(dp); //closing directory
            write(client, list, strlen(list)); //sending list to client by control port 
            memset(reply, 0, sizeof(reply));
            sleep(2);
            strcat(reply, "250 Requested file action okay, completed.");
            write(client, reply, strlen(reply)); //sending response to client
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "550 Requested action not taken.");
            write(client, reply, strlen(reply)); //if directory is not opened sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "225 Control connection is open.\n");
        if(dataConnection > 0){
            strcat(reply, "225 Data connection is open.");
        }
        write(client, reply, strlen(reply)); //if argument is not passed sending connection status to client
    }
    
}

//========================== function to send file to client ==========================

void send_file(char *fileName){
    int fd = open(fileName, O_RDONLY); //opening file in read mode
    if(fd != -1){ //checking if file is opened or not
        int bytes; //variable to store how many bytes read
        int fileSize = lseek(fd, 0, SEEK_END); //getting file size by lseek
        lseek(fd, 0, SEEK_SET); //setting cursor to starting of file
        char fileData[fileSize]; //character array of file size to store data
        memset(reply, 0, sizeof(reply)); 
        sprintf(reply, "%d", fileSize); //converting file size to string and storing it in reply variable
        write(client, reply, strlen(reply)); //sending file size to client
        if((bytes = read(fd, fileData, fileSize)) == fileSize){ //reading file and storing it to fileData
            write(dataConnection, fileData, bytes); //if file read sending it to client through data port
            sleep(2);
            memset(reply, 0, sizeof(reply));
            strcat(reply, "250 Requested file action okay, completed.");
            write(client, reply, strlen(reply)); //if sent sending success response to client
        }
        close(fd); //closing file
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "550 Requested action not taken. ");
        strcat(reply, fileName);
        strcat(reply, " file not found.");
        write(client, reply, strlen(reply)); //if file is not opened sending response to client
    }
}

//========================== function to handle RETR request ==========================

void retr(char * request){
    if(dataConnection > 0){ //checking data port is open or not
        char *token = strtok(request, " "); //splitting request by space
        if((token = strtok(NULL, " ")) != NULL){ //checking if argument is provided or not
            send_file(token); //if provided calling send_file function
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "501 Syntax error in parameters or arguments.");
            write(client, reply, strlen(reply)); //if argument is not provided sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "425 Can't open data connection.");
        write(client, reply, strlen(reply)); //if data port is not open sending response to client
    }
}

//========================== function to store file to server ==========================

int store_file(char *fileName, char *fileSize){
    int fd = open(fileName, O_WRONLY); //opening file in write mode
    if(fd != -1){ //if file opened it means file exists 
        close(fd); //closing file
        remove(fileName); //deleting old file in order to write new data 
    }

    fd = open(fileName, O_CREAT | O_WRONLY, 0777); //creating file to write new data
    if(fd != -1){ 
        char fileData[atoi(fileSize)]; //if file opened declaring char array to store data sent by client
        int bytes; //variable to store how many bytes read
        if((bytes = read(dataConnection, fileData, atoi(fileSize))) > 0){ //reading data sent by client from data port
            if(write(fd, fileData, bytes) == bytes){ //writing data to file
                memset(reply, 0, sizeof(reply));
                strcat(reply, "250 ");
                strcat(reply, fileName);
                strcat(reply, " file transfer complete");
                write(client, reply, strlen(reply)); //sending success response to client
            }
            else{
                memset(reply, 0, sizeof(reply));
                strcat(reply, "450 ");
                strcat(reply, fileName);
                strcat(reply, " file transfer not complete");
                write(client, reply, strlen(reply)); //if not written sending response to client
            }
        }
        close(fd); //closing file
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "452 ");
        strcat(reply, fileName);
        strcat(reply, " file not transfered");
        write(client, reply, strlen(reply)); //if file is not opened sending response to client
    }
}

//========================== function to handle STOR request ==========================

void stor(char *request){
    if(dataConnection > 0){ //checking data port is open or not
        char *token = strtok(request, " "); //splitting request by space
        char *localFile , *remoteFile; //variable to store file names from request 
        if((localFile = strtok(NULL, " ")) != NULL){ //first argument is local file which is in client 
            if((remoteFile = strtok(NULL, " ")) != NULL){ //second argument is remote file which is in server (this is an optional argument if not available than local file act as remote file)
                sleep(2);
                memset(reply, 0, sizeof(reply));
                read(client, reply, 4096); //reading size sent by client
                if(strncmp(reply, "-100000000", 10)){ //checking file size
                    store_file(remoteFile, reply); //if it is not negative calling store_file function passing remoteFile
                }
                else{
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "550 Requested action not taken.");
                    write(client, reply, strlen(reply)); //if it is negative sending response to client
                }
            }
            else{
                sleep(2);
                memset(reply, 0, sizeof(reply));
                read(client, reply, 4096); //reading size sent by client
                if(strncmp(reply, "-100000000", 10)){ //checking file size
                    store_file(localFile, reply); //if it is not negative calling store_file function passing localFile because remote file is not passed
                }
                else{
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "550 Requested action not taken.");
                    write(client, reply, strlen(reply)); //if it is negative sending response to client
                }
            }
            
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "501 Syntax error in parameters or arguments.");
            write(client, reply, strlen(reply)); //if argument is not provided sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "425 Can't open data connection.");
        write(client, reply, strlen(reply)); //if data port is not open sending response to client
    }
}

//========================== function to handle NOOP request ==========================

void noop(){
    memset(reply, 0, sizeof(reply));
    strcat(reply, "200 Command okay.");
    write(client, reply, strlen(reply)); //sending response to client
}

//========================== function to handle PWd request ==========================

void pwd(){
    char pwd[1024]; //variabe to store present working directory
    if(getcwd(pwd, 1024) != NULL){ //getting present working directory
        memset(reply, 0, sizeof(reply));
        strcat(reply, "257 ");
        strcat(reply, pwd);
        write(client, reply, strlen(reply)); //if got sending response to client
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "550 Requested action not taken.");
        write(client, reply, strlen(reply)); //if failed sending response to client
    }
}

//========================== function to append file to server ==========================

int append_file(char *fileName, char *fileSize){
    int fd = open(fileName, O_CREAT | O_APPEND | O_WRONLY, 0777); //opening file in append and write mode if it is not available than creating it
    if(fd != -1){ //
        char fileData[atoi(fileSize)]; //if file opened declaring char array to store data sent by client
        int bytes; //variable to store how many bytes read
        if((bytes = read(dataConnection, fileData, atoi(fileSize))) > 0){ //reading data sent by client from data port
            if(write(fd, fileData, bytes) == bytes){ //appending data to file
                memset(reply, 0, sizeof(reply));
                strcat(reply, "250 ");
                strcat(reply, fileName);
                strcat(reply, " file transfer complete");
                write(client, reply, strlen(reply)); //sending success response to client
            }
            else{
                memset(reply, 0, sizeof(reply));
                strcat(reply, "450 ");
                strcat(reply, fileName);
                strcat(reply, " file transfer not complete");
                write(client, reply, strlen(reply)); //if not appended sending response to client
            }
        }
        close(fd); //closing file
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "452 ");
        strcat(reply, fileName);
        strcat(reply, " file not transfered");
        write(client, reply, strlen(reply)); //if file is not opened sending response to client
    }
}

//========================== function to handle APPE request ==========================

void appe(char *request){
    if(dataConnection > 0){ //checking data port is open or not
        char *token = strtok(request, " "); //splitting request by space
        char *localFile , *remoteFile; //variable to store file names from request 
        if((localFile = strtok(NULL, " ")) != NULL){ //first argument is local file which is in client 
            if((remoteFile = strtok(NULL, " ")) != NULL){ //second argument is remote file which is in server (this is an optional argument if not available than local file act as remote file)
                sleep(2);
                memset(reply, 0, sizeof(reply));
                read(client, reply, 4096); //reading size sent by client
                if(strncmp(reply, "-100000000", 10)){ //checking file size
                    append_file(remoteFile, reply); //if it is not negative calling append_file function passing remoteFile
                }
                else{
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "550 Requested action not taken.");
                    write(client, reply, strlen(reply)); //if it is negative sending response to client
                }
            }
            else{
                sleep(2);
                memset(reply, 0, sizeof(reply));
                read(client, reply, 4096); //reading size sent by client
                if(strncmp(reply, "-100000000", 10)){ //checking file size
                    append_file(localFile, reply); //if it is not negative calling append_file function passing localFile because remote file is not passed
                }
                else{
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "550 Requested action not taken.");
                    write(client, reply, strlen(reply)); //if it is negative sending response to client
                }
            }
            
        }
        else{
            memset(reply, 0, sizeof(reply));
            strcat(reply, "501 Syntax error in parameters or arguments.");
            write(client, reply, strlen(reply)); //if argument is not provided sending response to client
        }
    }
    else{
        memset(reply, 0, sizeof(reply));
        strcat(reply, "425 Can't open data connection.");
        write(client, reply, strlen(reply)); //if data port is not open sending response to client
    }
}

//========================== function to handle REIN request ==========================

void rein(){
    if(dataConnection > 0){ //checking data port is open or not
        close(dataConnection); //if open closing data port
        close(dataSocket); //closing data socket
        dataConnection = -1; //resetting data port variable
        dataSocket = -1; //resetting data socket variable
    }
    flag = 0; //resetting flag to handle user login
    memset(reply, 0, sizeof(reply));
    strcat(reply, "226 Closing data connection.");
    write(client, reply, strlen(reply)); //sending response to client
}

//========================== function for child process created by server ==========================

void child(){
    char message[4096], *token;
    int n;
    while(1){ //infinite loop
        if((n = read(client, message, 4096)) > 0){ //reading request sent by client
            message[n-1] = '\0'; //replacing last character by NULL
            if(flag == 0){ //checking login status by flag
                if(!strncmp(message, "USER", 4)){ //if request is USER
                    flag = 1; //resetting flag
                    user(); //calling user function         
                }
                else if(!strncmp(message, "PID", 3) && !gotClient){ //if request is PID which is for synchronization and it is first time
                    gotClient = 1; //resetting gotClient
                    clientPid = atoi(message + 4); //getting client PID
                }
                else if(!strncmp(message, "QUIT", 4)){ //if request is QUIT
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "221 Service closing control connection.");
                    write(client, reply, strlen(reply)); //sending response to client
                    if(dataConnection > 0){ //checking data port is open or not
                        close(dataConnection); //if open closing data port
                        close(dataSocket); //closing data socket
                    }
                    close(client); //closing control port
                    exit(0); //exiting from server
                }
                else{
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "530 Not logged in.");
                    write(client, reply, strlen(reply)); //sending response if any other command sent while not logged in
                }
            }
            else{ //if USER logged in
                if(!strncmp(message, "USER", 4)){
                    user(); //calling user function if request is USER       
                }
                else if(!strncmp(message, "LIST", 4)){
                    list(message);  //calling list function if request is LIST
                }
                else if(!strncmp(message, "CWD", 3)){
                    cwd(message);  //calling cwd function if request is CWD
                }
                else if(!strncmp(message, "CDUP", 4)){
                    cdup(); //calling cdup function if request is CDUP
                }
                else if(!strncmp(message, "REIN", 4)){
                    rein(); //calling rein function if request is REIN
                }
                else if(!strncmp(message, "PORT", 4)){
                    port(message); //calling port function if request is PORT
                }
                else if(!strncmp(message, "STOR", 4)){
                    stor(message); //calling stor function if request is STOR
                }
                else if(!strncmp(message, "RETR", 4)){
                    retr(message); //calling retr function if request is RETR
                }
                else if(!strncmp(message, "APPE", 4)){
                    appe(message); //calling appe function if request is APPE
                }
                else if(!strncmp(message, "RNTO", 4)){
                    rnto(message); //calling rnto function if request is RNTO
                }
                else if(!strncmp(message, "RNFR", 4)){
                    rnfr(); //calling rnfr function if request is RNFR
                }
                else if(!strncmp(message, "PWD", 3)){
                    pwd(); //calling pwd function if request is PWD
                }
                else if(!strncmp(message, "DELE", 4)){
                    dele(message); //calling dele function if request is DELE
                }
                else if(!strncmp(message, "RMD", 3)){
                    rmd(message); //calling rmd function if request is RMD
                }
                else if(!strncmp(message, "MKD", 3)){
                    mkd(message); //calling mkd function if request is MKD
                }
                else if(!strncmp(message, "STAT", 3)){
                    stat_function(message); //calling stat_function function if request is STAT
                }
                else if(!strncmp(message, "NOOP", 4)){
                    noop(); //calling noop function if request is NOOP
                }
                else if(!strncmp(message, "QUIT", 4)){ //if request is QUIT
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "221 Service closing control connection.");
                    write(client, reply, strlen(reply)); //sending response to client
                    if(dataConnection > 0){ //checking data port is open or not
                        close(dataConnection); //if open closing data port
                        close(dataSocket); //closing data socket
                    }
                    close(client); //closing control port
                    exit(0); //exiting from server
                }
                else{
                    memset(reply, 0, sizeof(reply));
                    strcat(reply, "502 Command not implemented.");
                    write(client, reply, strlen(reply)); //sending response if any other command sent
                }
            }

        }
    }
}

//========================== main function ==========================

int main(int argc, char *argv[]){
    int status;
    struct sockaddr_in servAdd;

    signal(SIGINT, signal_handler); //changing handler of SIGINT 
    signal(SIGSTOP, signal_handler); //changing handler of SIGSTOP
    signal(SIGTSTP, signal_handler); //changing handler of SIGTSTP
    
    if(argc != 3){ //checking argument count it is 3 or not
        printf("Usage: %s -d [directory]\n", argv[0]); //if not printing error 
        exit(0); //exiting from server
    }

    if(!strncmp(argv[1], "-d", 2)){ //checking second argument is "-d"
        if (change_directory(argv[2]) == -1){ //if it is changing directory
            printf("Error: Cannot change directory"); //if directory is not changed printing error
            exit(0); //exiting from server
        }
    }
    
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){ //creating control socket 
        printf("Error: Cannot create socket\n"); //if not created printing error
        exit(1); //exiting from server
    }
    
    servAdd.sin_family = AF_INET; //using IPv4 protocol 
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY); //setting address
    servAdd.sin_port = htons((uint16_t)1220); //setting 1220 port for control conection
    
    bind(sd, (struct sockaddr*)&servAdd, sizeof(servAdd)); //binding socket to port
    
    listen(sd, 5); //listening through socket
    
    while(1){ //infinite loop
        if((client = accept(sd, (struct sockaddr *)NULL, NULL)) > 0){ //accepting client connection request
            printf("Got Client\n"); //if accepted printing message 
            if((pid = fork()) == 0) //creating child 
                child(); //if child calling child function
            childPID[childCount] = pid; //if parent storing child PID to array
            childCount++; //incrementing child count
            close(client); //closing control port in parent
            client = -1; //resetting control port variable
            for(int i = 0; i < childCount; i++){ //loop to check status of child
                if(waitpid(childPID[i], &status, WNOHANG) == childPID[i]){ //checking if status is changed or not
                    if(i == childCount - 1){ //if changed checking it is last child or not
                        childCount--; //if it is decrementing child count to remove entry
                    }
                    else{ 
                        for(int j = i; j < childCount - 1; j++){ //if it is child between the array
                            childPID[j] = childPID[j+1]; //replacing entry
                        }
                        childCount--; //decrementing child count
                        i--; //decrementing index variable because removed entry and need to start from same position
                    }
                }
            }
        }
    }
}
