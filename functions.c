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
void sendErrPac(int sockfd, char *errMsg, char *errPac, struct sockaddr_storage their_addr, socklen_t addr_len) {

	uint16_t host, network;
    char *perrP = errPac;
    int sbytes;

    host = ERR;
    network = htons(host);
    memcpy(perrP, (char *) &network, 2);
    perrP += 2;

    host = 0;
    network = htons(host);
    memcpy(perrP, (char *) &network, 2);
    perrP += 2;

    memcpy(perrP, errMsg, strlen(errMsg));
    perrP += strlen(errMsg);

    *perrP = '\0';
    perrP++;

    if ((sbytes = sendto(sockfd, errPac, strlen(errMsg) + 4, 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    perror("fail to open the file!");
    exit(1);
}


// int read_netascii(FILE *fstream, char *pd, char *pnextchar) {
//     int count;
//     char nc = *pnextchar;
//     char c;
//     for (count = 0; count < MAXBYTES; count++) {
//         if (nc >= 0) {
//             *(pd + count) = nc;
//             nc = -1;
//             continue;
//         }
//
//         c = getc(fstream);
//
//         if (c == EOF) {
//             if (ferror(fstream)) {
//                 printf("%s\n", "read err from getc on local file");
//             }
//             return  count;
//         } else if (c == '\n') {
//             c = '\r';
//             nc = '\n';
//         } else if (c == '\r') {
//             nc = '\0';
//         } else {
//             nc = -1;
//         }
//         *(pd + count) = c;
//         count;
//     }
// }
