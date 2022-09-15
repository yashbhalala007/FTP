#include <stdio.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

int dataConnection = -1, server; //variables for dataport and control port

//========================== function to handle signal ==========================

void signal_handler(int signo){ 
    char reply[1024]; 
    write(server, "QUIT\n", 5); //sending QUIT request to server 
    memset(reply, 0, sizeof(reply)); //resetting buffer
    read(server, reply, 4096); //reading response of server
    printf("**************************** SERVER RESPONSE ****************************\n");
    printf("%s\n", reply); //writing response of server
    printf("\n**************************** SERVER RESPONSE END ****************************\n");
    if(dataConnection > 0){ //checking if data port is open or not
        close(dataConnection); //if open closing it
    }
    close(server); //closing control port
    exit(0); //exiting client
}

//========================== function to connect data port ==========================

int connect_to_port(char *newPort){
    if(dataConnection < 0){ //checking data port is already available
        struct sockaddr_in dataAdd; 
        
        if((dataConnection = socket(AF_INET, SOCK_STREAM, 0)) < 0){ //if not creating socket for data port and checking if it is created or not
            return -1; //if not returning -1
        }
        
        dataAdd.sin_family = AF_INET; //using IPv4 protocol 
        dataAdd.sin_port = htons(atoi(newPort)); //setting new port for data transfer
        
        while(1){ //loop until connection is successful
            if(connect(dataConnection, (struct sockaddr *) &dataAdd, sizeof(dataAdd)) < 0){ //trying to connect to data port
                continue; //if not connected continue in loop
            }
            return 0; //if connected returning 0
        }
    }
}

//========================== function to send file to server ==========================

void send_file(char *fileName){
    int fd = open(fileName, O_RDONLY); //opening file in read mode
    if(fd != -1){ //checking if file is opened or not
        int bytes; //variable to store how many bytes read
        int fileSize = lseek(fd, 0, SEEK_END); //getting file size by lseek
        lseek(fd, 0, SEEK_SET); //setting cursor to starting of file
        char fileData[fileSize]; //character array of file size to store data
        char reply[10]; //character array to send size of file to server
        memset(reply, 0, sizeof(reply));
        sprintf(reply,"%d", fileSize); //converting file size to string and storing it in reply variable
        sleep(2);
        write(server, reply, strlen(reply)); //sending file size to server
        if((bytes = read(fd, fileData, fileSize)) == fileSize){ //reading file and storing it to fileData
            write(dataConnection, fileData, bytes); //if file read sending it to server through data port
            memset(fileData, 0, sizeof(fileData));
        }
        close(fd); //closing file
    }
    else{
        write(server, "-100000000", 10); //if file is not open sending negative value instead of file size to server so that server can take appropiate steps
    }
}

//========================== function to hadle STOR command ==========================

void stor(char *request){
    char *token = strtok(request, " "); //splitting request by space
    if((token = strtok(NULL, " ")) != NULL){ //getting second argument from request which is file name
        send_file(token); //if there is second argument available than calling send_file function and passing file name as argument
    }
}

//========================== function to store file in client ==========================

void store_file(char *fileName, char *fileSize){
    int fd = open(fileName, O_WRONLY); //opening file in write mode
    if(fd != -1){ //if file opened it means file exists 
        close(fd); //closing file
        remove(fileName); //deleting old file in order to write new data 
    }

    fd = open(fileName, O_CREAT | O_WRONLY, 0777); //creating file to write new data
    if(fd != -1){ 
        char fileData[atoi(fileSize)]; //if file opened declaring char array to store data sent by server
        int bytes; //variable to store how many bytes read
        if((bytes = read(dataConnection, fileData, atoi(fileSize))) > 0){ //reading data sent by server from data port
            write(fd, fileData, bytes); //writing data to file
        }
        close(fd); //closing file
    }
}

//========================== function to hadle RETR command ==========================

void retr(char *request, char *fileSize){
    char *token = strtok(request, " "); //splitting request by space
    char *localFile , *remoteFile; //variable to store file names from request 
    if((remoteFile = strtok(NULL, " ")) != NULL){ //first argument is remote file which is in server 
        if((localFile = strtok(NULL, " ")) != NULL){ //second argument is local file which is in client (this is an optional argument if not available than remote file act as local file)
            store_file(localFile, fileSize); //if second file available than passing it to store file function
        }
        else{
            store_file(remoteFile, fileSize); //otherwise passing first argument
        }
    }
}

//========================== main function ==========================

int main(int argc, char *argv[]){
    char message[4096], reply[4096], *token;
    int n, fileTransfer, flag = 0;
    struct sockaddr_in servAdd;
    
    signal(SIGINT, signal_handler); //changing handler of SIGINT 
    signal(SIGSTOP, signal_handler); //changing handler of SIGSTOP
    signal(SIGTSTP, signal_handler); //changing handler of SIGTSTP

    if((server=socket(AF_INET, SOCK_STREAM, 0)) < 0){ //creating socket for control connection
        fprintf(stderr, "Cannot create socket\n"); //if not created printing error
        exit(1); //and exiting from client
    }
    
    servAdd.sin_family = AF_INET; //using IPv4 protocol 
    servAdd.sin_port = htons((uint16_t)1220); //setting 1220 port for control conection
    
    if(connect(server, (struct sockaddr *) &servAdd, sizeof(servAdd)) < 0){ //trying to connect to server through 1220 port
        fprintf(stderr, "connect() has failed, exiting\n"); //if not connected printing error
        exit(3); //exiting from client
    }

    sprintf(message, "PID %d\n", getpid()); //getting PID of client 
    write(server, message, strlen(message)); //sending PID of client to server to handle ctrl+C or ctrl+Z at client side 

    while(1){ //infinite loop
        memset(message, 0, sizeof(message)); //resetting buffer
        if((n = read(STDIN_FILENO, message, 255)) > 0){ //reading request from standard input
            write(server, message, n); //sending request to server
            message[n-1] = '\0'; //changing last character to null character
            if(!strncmp(message, "USER", 4)){ //if request was USER 
                flag = 1; //setting flag to 1 which will allow user to execute any request
            }
            else if((!strncmp(message, "STOR", 4) || !strncmp(message, "APPE", 4)) && flag){ //if request was STOR or APPE and flag is 1
                if(dataConnection > 0){ //checking data connection
                    stor(message); //if data port is open calling function which will handle request
                }
            }
        }

        if(!strncmp(message, "PORT", 4) && flag){ //if request was PORT and flag is 1
            token = strtok(message, " "); //splitting request by space
            if((token = strtok(NULL, " ")) != NULL){ //getting port specified in first argument
                if (connect_to_port(token) == -1){ //sending it to connect_to_port function
                    fprintf(stderr, "Cannot create data socket\n"); //if not connected printing error
                }
            }
        }

        if((n = read(server, reply, 4096)) > 0){ //reading response sent by server
            if(!strncmp(message, "LIST", 4) && flag){ //if request was LIST and flag is 1
                if(dataConnection > 0){ //checking data connection is open or not
                    if(!strncmp(reply, "125", 3)){ //if open checking response is 125
                        printf("**************************** SERVER RESPONSE ****************************\n");
                        printf("%s\n", reply); //if 125 printing response
                        memset(reply, 0, sizeof(reply));
                        if((n = read(dataConnection, reply, 4096)) > 0){ //reading data sent by server on data port
                            printf("%s\n", reply); //print output of LIST
                            memset(reply, 0, sizeof(reply));
                        }
                        if((n = read(server, reply, 4096)) > 0){ //reading response from server
                            if(!strncmp(reply, "250", 3)){ //if 250
                                printf("%s\n", reply); //printing response
                                printf("\n**************************** SERVER RESPONSE END ****************************\n");
                                memset(reply, 0, sizeof(reply));
                                continue; //continue in loop
                            }
                        }    
                    }
                }            
            }
            else if(!strncmp(message, "STAT", 4) && flag){ //if request was STAT and flag is 1
                if(!strncmp(reply, "125", 3)){ //checking response is 125
                    printf("**************************** SERVER RESPONSE ****************************\n");
                    printf("%s\n", reply); //if 125 printing response
                    memset(reply, 0, sizeof(reply));
                    if((n = read(server, reply, 4096)) > 0){ //reading data sent by server on control port
                        printf("%s\n", reply); //print output of STAT
                        memset(reply, 0, sizeof(reply));
                    }
                    if((n = read(server, reply, 4096)) > 0){ //reading response from server
                        if(!strncmp(reply, "250", 3)){ //if 250
                            printf("%s\n", reply); //printing response
                            printf("\n**************************** SERVER RESPONSE END ****************************\n");
                            memset(reply, 0, sizeof(reply));
                            continue; //continue in loop
                        }
                    }    
                }            
            }
            else if(!strncmp(message, "RETR", 4) && flag){ //if request was RETR and flag is 1
                if(dataConnection > 0){ //checking data connection is open or not
                    if(strncmp(reply, "550", 3)){ //if open checking response is 550
                        retr(message, reply); //if not calling function to handle RETR request and passing request and response as arguments
                        if((n = read(server, reply, 4096)) > 0){ //reading response from server
                            if(!strncmp(reply, "250", 3)){ //if 250
                                printf("**************************** SERVER RESPONSE ****************************\n");
                                printf("%s\n", reply); //printing response
                                printf("\n**************************** SERVER RESPONSE END ****************************\n");
                                memset(reply, 0, sizeof(reply));
                                continue; //continue in loop
                            }
                        }    
                    }
                }            
            }
            else if(!strncmp(message, "REIN", 4) && flag){ //if request was REIN and flag is 1
                if(dataConnection > 0){ //checking data connection is open or not
                    close(dataConnection); //if open closing data port
                    dataConnection = -1; //resetting variable
                }            
                flag = 0; //resetting flag to change state of USER
            }
            
            if(!strncmp(reply, "221", 3)){ //if response if 221
                printf("**************************** SERVER RESPONSE ****************************\n");
                printf("%s\n", reply); //printing response
                printf("\n**************************** SERVER RESPONSE END ****************************\n");
                memset(reply, 0, sizeof(reply));
                if(dataConnection > 0){ //checking data connection is open or not
                    close(dataConnection); //if open closing data port
                }
                close(server); //closing control connection
                exit(0); //exiting from client 
            }

            printf("**************************** SERVER RESPONSE ****************************\n");
            printf("%s\n", reply); //printing response
            printf("\n**************************** SERVER RESPONSE END ****************************\n");
            memset(reply, 0, sizeof(reply));
         }
    }
}