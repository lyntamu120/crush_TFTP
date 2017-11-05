/*
** server.c
*/

// opcode
// 1     Read request (RRQ)
// 2     Write request (WRQ)
// 3     Data (DATA)
// 4     Acknowledgment (ACK)
// 5     Error (ERROR)

//Errcode
// Value     Meaning
//
// 0         Not defined, see error message (if any).
// 1         File not found.
// 2         Access violation.
// 3         Disk full or allocation exceeded.
// 4         Illegal TFTP operation.
// 5         Unknown transfer ID.
// 6         File already exists.
// 7         No such user.

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERR 5

#define MYPORT "4950" // the port users will be connecting to
#define MAXBUFLEN 512
#define MAXBYTES 512
#define MODE1 "NETASCII"
#define MODE2 "OCTET"

#include "functions.c"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    struct sockaddr_in addr;
    // int ranPort = MYPORT;
    int rbytes;
    int opcode;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    char filename[MAXBUFLEN], mode[512];
    char writeData[MAXBUFLEN];
    char recvErrMsg[512];
    char dataPac[516], dataGram[512], ackPac[10];
    char *pdataP;
    char *perrP;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    char c;
    uint16_t host, network;
    int sbytes;
    int mode_flag = 0;
    FILE* fstream;
    int count;
    int yes = 1;
    char nextchar = -1;
    char prev = -1;
    int numOfTimeouts = 0;
    int isEnd = 0;
    int numOfBlock, numOfACK;
    int errCode;
    char errPac[517];
    char *errMsg;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
            perror("Server: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server: bind");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Server: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(servinfo);
    printf("listener: waiting to recvfrom...\n");


    while(1) {
        addr_len = sizeof their_addr;
        //receive the RRQ or WRQ
        if ((rbytes = recvfrom(sockfd, buf, MAXBUFLEN, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        if (!fork()) {
            close(sockfd); // child doesn't need the main sockfd

            //create new socketfd
            memset(&addr, 0, sizeof addr);

            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr("0.0.0.0");
            addr.sin_port = htons(0); //assign an ephemeral port

            if ((new_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
                perror("child process server: socket");
                exit(1);
            }

            if (setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                perror("child process server: setsockopt");
                exit(1);
            }

            if (bind(new_fd, (struct sockaddr *)&addr, sizeof addr) == -1) {
                close(new_fd);
                perror("child process server: bind");
                exit(1);
            }
            
            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //begin handling requests

            //decode opcode
            memcpy((char *) &network, &buf[0], 2);
            host = ntohs(network);

            //printf("The opcode is: %i\n", host);

            //decode filename and mode
            strcpy(filename, &buf[2]);
            strcpy(mode, &buf[2 + strlen(filename) + 1]);

            //normalization the mode
            int i = 0;
            while(mode[i]) {
              mode[i] = toupper(mode[i]);
              i++;
            }

            printf("Server: filename is \"%s\"\n", filename);
            printf("Server: mode is \"%s\"\n", mode);

            if (strcmp(MODE1, mode) == 0) {
                //in netascii mode
                mode_flag = 1;
            } else if (strcmp(MODE2, mode) == 0) {
                //in octet mode
                mode_flag = 2;
            } else {
                //send error message
                sendErrPac(4, new_fd, "Illegal TFTP operation.", &errPac[0], their_addr, addr_len);
            }

            //The first packet can only be RRQ or WRQ

            if (host == RRQ) {
                //handle RRQ
                fstream = fopen(filename, "r");

                if (fstream == NULL) {
                    //send err message
                    sendErrPac(1, new_fd, "Fail to open the file!", &errPac[0], their_addr, addr_len);
                    perror("openfile");
                    exit(1);
                } else {
                    //send the first packet
                    printf("%s\n", "File open successfully!");

                    count = readForBothMode(mode_flag, fstream, &dataGram[0], &nextchar);

                    numOfBlock = 1;

                    sendDataPac(count, numOfBlock, new_fd, &dataPac[0], &dataGram[0], their_addr, addr_len);


                }
            } else if (host == WRQ) {
                //handle WRQ
                fstream = fopen(filename, "w");
                if (fstream == NULL) {
                    //send err message
                    sendErrPac(0, new_fd, "Fail to open the file!", &errPac[0], their_addr, addr_len);
                    perror("openfile");
                    exit(1);
                } else {
                    //send ACK 0
                    numOfACK = 0;

                    sendACKPac(new_fd, numOfACK, &ackPac[0], their_addr, addr_len);
                }
            }

            //handle the rest of the packets
            while(1) {

                    //check if timeout
                    while (readable_timeout(new_fd, 1) == 0) {
                        printf("Timeout!\n");
                        numOfTimeouts++;
                        if (numOfTimeouts >= 10) {
                            isEnd = 1;
                            break;
                        }
                    }

                    if (isEnd == 1) {
                        printf("There is no response from the client!\n");
                        close(new_fd);
                        exit(0);
                    } else {
                        numOfTimeouts = 0;
                    }

                    memset(buf, 0, sizeof buf);

                    //try to receive the rest of the packet
                    if ((rbytes = recvfrom(new_fd, buf, MAXBUFLEN, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("recvfrom");
                        exit(1);
                    }

                    //handle ACK
                    if (buf[1] == ACK) {
                        //obtain packet number
                        memcpy((char *) &network, &buf[2], 2);
                        host = ntohs(network);

                        if (host != numOfBlock) {
                            printf("%s\n", "duplicate ACK received!");
                        } else {

                            if (count < 512) {
                                printf("This is the last block!\n");
                                fclose(fstream);
                                break;
                            }

                            //received the corret ACK
                            //keep sending the next dataPac

                            //Wrap-Around
                            numOfBlock = (numOfBlock + 1) % 65536;

                            count = readForBothMode(mode_flag, fstream, &dataGram[0], &nextchar);

                            sendDataPac(count, numOfBlock, new_fd, &dataPac[0], &dataGram[0], (struct sockaddr *)&their_addr, addr_len);

                        }
                    } else if (buf[1] == DATA) {

                        //obtain packet number
                        memcpy((char *) &network, &buf[2], 2);
                        host = ntohs(network);

                        if (host == numOfACK + 1) {
                            //Wrap-Around
                            numOfACK = (numOfACK + 1) % 65536;;

                            sendACKPac(new_fd, numOfACK, &ackPac[0], their_addr, addr_len);

                            //write data into a file
                            //int fprintf(FILE *stream, const char *format, ...)
                            memcpy(writeData, &buf[4], rbytes - 4);
                            printf("The received bytes are: %d\n", rbytes - 4);

                            // fprintf(fstream, "%s\n", writeData);
                            writeForBothMode(mode_flag, fstream, writeData, rbytes - 4, &prev);

                            if (rbytes < 512) {
                                printf("%s\n", "Sent the last ACK!");
                                fclose(fstream);
                                break;
                            }

                        } else {
                            //the blockNum is the previous packet
                            printf("duplicate data packet received!\n");
                        }

                    } else if (buf[1] == ERR){
                        //obtain errcode
                        memcpy((char *) &network, &buf[2], 2);
                        host = ntohs(network);

                        //obtain errmessage
                        strcpy(recvErrMsg, &buf[4]);

                        printf("Error message received, error code: %i\n", host);
                        printf("Error message received, error message: %s\n", recvErrMsg);
                        exit(0);

                    }
                }





            close(new_fd);
            exit(0);
        } 


        // //decode opcode
        // memcpy((char *) &network, &buf[0], 2);
        // host = ntohs(network);

        // //printf("The opcode is: %i\n", host);

        // //decode filename and mode
        // strcpy(filename, &buf[2]);
        // strcpy(mode, &buf[2 + strlen(filename) + 1]);

        // //normalization the mode
        // int i = 0;
        // while(mode[i]) {
        //   mode[i] = toupper(mode[i]);
        //   i++;
        // }

        // printf("Server: filename is \"%s\"\n", filename);
        // printf("Server: mode is \"%s\"\n", mode);

        // if (strcmp(MODE1, mode) == 0) {
        //     //in netascii mode
        //     mode_flag = 1;
        // } else if (strcmp(MODE2, mode) == 0) {
        //     //in octet mode
        //     mode_flag = 2;
        // } else {
        //     //send error message
        //     sendErrPac(4, sockfd, "Illegal TFTP operation.", &errPac[0], their_addr, addr_len);
        // }

        // //The first packet can only be RRQ or WRQ

        // if (host == RRQ) {
        //     //handle RRQ
        //     fstream = fopen(filename, "r");

        //     if (fstream == NULL) {
        //         //send err message
        //         sendErrPac(1, sockfd, "Fail to open the file!", &errPac[0], their_addr, addr_len);
        //         perror("openfile");
        //         exit(1);
        //     } else {
        //         //send the first packet
        //         printf("%s\n", "File open successfully!");

        //         count = readForBothMode(mode_flag, fstream, &dataGram[0], &nextchar);

        //         numOfBlock = 1;

        //         sendDataPac(count, numOfBlock, sockfd, &dataPac[0], &dataGram[0],their_addr, addr_len);
        //     }
        // } else if (host == WRQ) {
        //     //handle WRQ
        //     fstream = fopen(filename, "w");
        //     if (fstream == NULL) {
        //         //send err message
        //         sendErrPac(0, sockfd, "Fail to open the file!", &errPac[0], their_addr, addr_len);
        //         perror("openfile");
        //         exit(1);
        //     } else {
        //         //send ACK 0
        //         numOfACK = 0;

        //         sendACKPac(sockfd, numOfACK, &ackPac[0], their_addr, addr_len);
        //     }
        // }

        // //handle the rest of the packets
        // while(1) {

        //         //check if timeout
        //         while (readable_timeout(sockfd, 1) == 0) {
        //             printf("Timeout!\n");
        //             numOfTimeouts++;
        //             if (numOfTimeouts >= 10) {
        //                 isEnd = 1;
        //                 break;
        //             }
        //         }

        //         if (isEnd == 1) {
        //             printf("There is no response from the client!\n");
        //             break;
        //         } else {
        //             numOfTimeouts = 0;
        //         }

        //         memset(buf, 0, sizeof buf);

        //         //try to receive the rest of the packet
        //         if ((rbytes = recvfrom(sockfd, buf, MAXBUFLEN, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        //             perror("recvfrom");
        //             exit(1);
        //         }

        //         //handle ACK
        //         if (buf[1] == ACK) {
        //             //obtain packet number
        //             memcpy((char *) &network, &buf[2], 2);
        //             host = ntohs(network);

        //             if (host != numOfBlock) {
        //                 printf("%s\n", "duplicate ACK received!");
        //             } else {

        //                 if (count < 512) {
        //                     printf("This is the last block!\n");
        //                     fclose(fstream);
        //                     break;
        //                 }

        //                 //received the corret ACK
        //                 //keep sending the next dataPac

        //                 //Wrap-Around
        //                 numOfBlock = (numOfBlock + 1) % 65536;

        //                 count = readForBothMode(mode_flag, fstream, &dataGram[0], &nextchar);

        //                 sendDataPac(count, numOfBlock, sockfd, &dataPac[0], &dataGram[0],their_addr, addr_len);

        //             }
        //         } else if (buf[1] == DATA) {

        //             //obtain packet number
        //             memcpy((char *) &network, &buf[2], 2);
        //             host = ntohs(network);

        //             if (host == numOfACK + 1) {
        //                 //Wrap-Around
        //                 numOfACK = (numOfACK + 1) % 65536;;

        //                 sendACKPac(sockfd, numOfACK, &ackPac[0], their_addr, addr_len);

        //                 //write data into a file
        //                 //int fprintf(FILE *stream, const char *format, ...)
        //                 memcpy(writeData, &buf[4], rbytes - 4);
        //                 printf("The received bytes are: %d\n", rbytes - 4);

        //                 // fprintf(fstream, "%s\n", writeData);
        //                 writeForBothMode(mode_flag, fstream, writeData, rbytes - 4, &prev);

        //                 if (rbytes < 512) {
        //                     printf("%s\n", "Sent the last ACK!");
        //                     fclose(fstream);
        //                     break;
        //                 }

        //             } else {
        //                 //the blockNum is the previous packet
        //                 printf("duplicate data packet received!\n");
        //             }

        //         } else if (buf[1] == ERR){
        //             //obtain errcode
        //             memcpy((char *) &network, &buf[2], 2);
        //             host = ntohs(network);

        //             //obtain errmessage
        //             strcpy(recvErrMsg, &buf[4]);

        //             printf("Error message received, error code: %i\n", host);
        //             printf("Error message received, error message: %s\n", recvErrMsg);
        //             exit(0);

        //         }
        //     }

    }


    close(sockfd);
    return 0;
}
