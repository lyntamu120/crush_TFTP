//timeout function
int readable_timeout(int fd, int sec) {
    fd_set rset;
    struct timeval tv;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);

    tv.tv_sec = sec;
    tv.tv_usec = 0;

    return (select(fd + 1, &rset, NULL, NULL, &tv));
}


//send error pac to the client
void sendErrPac(int errCode, int sockfd, char *errMsg, char *errPac, struct sockaddr_storage their_addr, socklen_t addr_len) {
	printf("%s\n", "Inside sendErrPac function!");

	uint16_t host, network;
    char *perrP = errPac;
    int sbytes;

    host = ERR;
    network = htons(host);
    memcpy(perrP, (char *) &network, 2);
    perrP += 2;

    host = errCode;
    network = htons(host);
    memcpy(perrP, (char *) &network, 2);
    perrP += 2;

    memcpy(perrP, errMsg, strlen(errMsg));
    perrP += strlen(errMsg);
    printf("The errMsgis: %s\n", errMsg);
    printf("The length of the errMsgis: %lu\n", strlen(errMsg));
    *perrP = '\0';
    perrP++;

    if ((sbytes = sendto(sockfd, errPac, strlen(errMsg) + 5, 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
}

//send ACK pac to the client
void sendACKPac(int sockfd, int numOfACK, char *ackPac, struct sockaddr_storage their_addr, socklen_t addr_len) {
	printf("%s\n", "Inside sendACKPac function!");

	uint16_t host, network;
	char *packP = ackPac;
	int sbytes;

	host = ACK;
	network = htons(host);
	memcpy(packP, (char *) &network, 2);
	packP += 2;

	host = numOfACK;
	network = htons(host);
	memcpy(packP, (char *) &network, 2);
	packP += 2;

	if ((sbytes = sendto(sockfd, ackPac, 4, 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
        perror("talker: sendto");
        exit(1);
    } else {
        printf("ACK %d send successfully!\n", numOfACK);
    }

}

//send the data pac to the client
void sendDataPac(int count, int numOfBlock, int sockfd, char *dataPac, char *dataGram, struct sockaddr_storage their_addr, socklen_t addr_len) {
	printf("%s\n", "Inside sendDataPac function!");
	uint16_t host, network;
	char *pdataP = dataPac;
	int sbytes;

    host = DATA;
    network = htons(host);
    memcpy(pdataP, (char *) &network, 2);
    pdataP += 2;

    host = numOfBlock;
    network = htons(host);
    memcpy(pdataP, (char *) &network, 2);
    pdataP += 2;

    memcpy(pdataP, dataGram, count);

    if ((sbytes = sendto(sockfd, dataPac, count + 4, 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
        perror("talker: sendto");
        exit(1);
    } else {
        printf("The sended bytes are: %d\n", count);
        printf("Block %d send successfully!\n", numOfBlock);
    }
}

//the read function
int readForBothMode(int mode_flag, FILE *fstream, char *dataGram, char *pnextchar) {
	printf("%s\n", "Inside readForBothMode function!");
	int count;
	char nextchar = *pnextchar;
	char c;

    for (count = 0; count < MAXBYTES; count++) {
        if (mode_flag == 1) {
            //in netascii mode, change the initial file content
            if (nextchar >= 0) {
                dataGram[count] = nextchar;
                nextchar = -1;
                continue;
            }

            c = getc(fstream);

            if (c == EOF) {
                if (ferror(fstream)) {
                    printf("%s\n", "read err from getc on local file");
                }
                break;
            } else if (c == '\n') {
                c = '\r';
                nextchar = '\n';
            } else if (c == '\r') {
                nextchar = '\0';
            } else {
                nextchar = -1;
            }
            dataGram[count] = c;
        } else {
            //in octet mode
            c = getc(fstream);
            if (c == EOF) {
                if (ferror(fstream)) {
                    printf("%s\n", "read err from getc on local file");
                }
                break;
            }
            dataGram[count] = c;
        }
    }

    *pnextchar = nextchar;
    return count;
}

//the write function
void writeForBothMode(int mode_flag, FILE *fstream, char *data, int writeLen, char *pPrev) {
	printf("%s\n", "Inside writeForBothMode function!");
	int count;
	char ch;
	char prev = *pPrev;
	int actual = 0;

	for(count = 0 ; count < writeLen; count++) {	
		if (mode_flag == 1) {
			//in netassci mode
			ch = data[count];
	   		if (ch == '\r') {
	   			prev = '\r';
	   			continue;
	   		}

	   		if (prev == '\r') {
	   			if (ch == '\0') {
	   				ch = prev;
	   			}
	   			prev = -1;
	   		} 
	      	putc(ch, fstream);
		} else {
			//in octet mode
			ch = data[count];
			putc(ch, fstream);
		}
		actual++;	
   }
   printf("Write %d bytes into the file!\n", actual);
   *pPrev = prev;
}
