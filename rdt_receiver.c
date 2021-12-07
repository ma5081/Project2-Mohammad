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
struct timeval tp;
tcp_packet *recvpkt;
tcp_packet *sndpkt;
int rsize=0; // amount of packets to get
short* buffered; // an array-type system of bools to check the packets that are buffered
int dupes=0;
int prevack;
FILE *csv;

void csvTime(int dSize, int sNo)
{
    gettimeofday(&tp, NULL);
    VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);
    fprintf(csv, "%lu, %d, %d\n", tp.tv_sec, dSize, sNo);
}

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    int lastack = 0;
    int prevack = 0;

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
    csv = fopen("TPT.csv", "w");
    if (csv == NULL) {
        error("TPT.csv");
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

    // Handshake with other side
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
        buffered = calloc(rsize,sizeof(short)); // dynamic array
    }
    else
    {
        buffered = malloc(0); // setting buffered to bypass errors
    }

    printf("%s: %d\n", "Packets Needed", rsize);

    sndpkt = make_packet(0);
    sndpkt->hdr.ackno = 0; // initialize the file transfer
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
        csvTime(recvpkt->hdr.data_size,recvpkt->hdr.seqno);
        sndpkt = make_packet(0);
        prevack = lastack;
        if ((recvpkt->hdr.data_size == 0 && recvpkt->hdr.seqno >= 0)||rsize<=0) // if received packet is empty and it is not the handshake or if the file to be received is empty
        {
            int ender = 0;
            for (int i = 0; i < rsize; i++) // check all the packets to make sure they are all received
            {
                if(!*(buffered+i)){ender = i; break;}
            }
            if(rsize<=0) // if the file is empty
            {
                // end connection and sends ACK to the other side
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
                fclose(csv);
                break;
            }
            lastack = ender*DATA_SIZE; // after checking all the packets, sends an ACK for the missing packet (safety net and to account for large window sizes)
        }
        else if(recvpkt->hdr.seqno < 0) // if there was a retransmit of the handshake
        {
            lastack = 0;
        }
        else
        {
            int rpack = recvpkt->hdr.seqno/DATA_SIZE;
            sndpkt->hdr.seqno = recvpkt->hdr.seqno;
            if(!*(buffered+rpack)) // if not already buffered
            {
                fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
                fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
                *(buffered+rpack) = 1; // set the packet as buffered
                if (lastack == recvpkt->hdr.seqno) // if the packet is the packet that was expected
                {
                    for(int i=rpack; i<rsize; i++) // check the next missing piece
                    {
                        if(!*(buffered+i)){rpack = i; break;}
                        rpack = rsize; // set to rsize unless breaking the loop with the if statement
                    }
                    lastack = rpack*DATA_SIZE;
                }
            }
        }
        if(prevack == lastack)
            dupes++;
        else
            dupes = 0;
        if(dupes <= 5)
        {
            sndpkt->hdr.ackno = lastack;
            sndpkt->hdr.ctr_flags = ACK;
            printf("%s: %d\n", "sent the following ACK", lastack);
            if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
                    (struct sockaddr *) &clientaddr, clientlen) < 0)
            {
                error("ERROR in sendto");
            }
        }
    }
    return 0;
}
