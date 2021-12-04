#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"


/*
 * You ar required to change the implementation to support
 * window size greater than one.
 * In the currenlt implemenetation window size is one, hence we have
 * onlyt one send and receive packet
 */
tcp_packet *recvpkt;
tcp_packet *sndpkt;
int rsize=0;
short* buffered;
int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    struct timeval tp;
    int lastack = 0;

    /*
     * check command line arguments
     */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr,
                sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /*
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);

    if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
            (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0)
    {
        error("ERROR in recvfrom");
    }
    recvpkt = (tcp_packet *) buffer;
    assert(get_data_size(recvpkt) <= DATA_SIZE);
    rsize = recvpkt->hdr.ackno;
    if(rsize)
    {
        buffered = calloc(rsize,sizeof(short));
    }
    else
    {
        buffered = malloc(0);
    }

    printf("%s: %d\n", "Packets Needed", rsize);

    sndpkt = make_packet(0);
    sndpkt->hdr.ackno = 0;
    sndpkt->hdr.ctr_flags = ACK;
    printf("%s: %d\n", "sent the following ACK", lastack);
    if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
            (struct sockaddr *) &clientaddr, clientlen) < 0)
    {
        error("ERROR in sendto");
    }

    while (1)
    {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0)
        {
            error("ERROR in recvfrom");
        }
        recvpkt = (tcp_packet *) buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        sndpkt = make_packet(0);
        if ((recvpkt->hdr.data_size == 0 && recvpkt->hdr.seqno >= 0)||rsize<=0)
        {
            int ender = 0;
            for (int i = 0; i < rsize; i++)
            {
                if(!*(buffered+i)){ender = i; break;}
            }
            if(rsize<=0)
            {
                sndpkt->hdr.ackno = 0;
                sndpkt->hdr.ctr_flags = ACK;
                printf("%s: %d\n", "sent the following ACK", lastack);
                if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
                        (struct sockaddr *) &clientaddr, clientlen) < 0)
                {
                    error("ERROR in sendto");
                }
            }
            if(!ender)
            {
                printf("ENDED!\n");
                //VLOG(INFO, "End Of File has been reached");
                fclose(fp);
                break;
            }
            lastack = ender*DATA_SIZE;
        }
        else if(recvpkt->hdr.seqno < 0)
        {
            lastack = 0;
        }
        else
        {
            gettimeofday(&tp, NULL);
            VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);

            fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
            fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
            int rpack = recvpkt->hdr.seqno/DATA_SIZE;
            sndpkt->hdr.seqno = recvpkt->hdr.seqno;
            *(buffered+rpack) = 1;
            if (lastack == recvpkt->hdr.seqno)
            {
                lastack = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
            }
        }
        sndpkt->hdr.ackno = lastack;
        sndpkt->hdr.ctr_flags = ACK;
        printf("%s: %d\n", "sent the following ACK", lastack);
        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
                (struct sockaddr *) &clientaddr, clientlen) < 0)
        {
            error("ERROR in sendto");
        }
    }
    return 0;
}
