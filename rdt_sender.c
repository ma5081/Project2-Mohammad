#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  120 //milli second

int lastsend=-1;
int rsend = 0;
long send_base=0;
int window_size = 1;
int looper = 1;
int len = 0;

int rsize = 0;

int lastav;
int dupe = 0;
int ssthresh = 64;
int ss = 1;
int cc = 0;

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer;
tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;

char buffer[DATA_SIZE];
FILE *fp;



void end_packets() // close connection
{
    looper = 0;
    sndpkt = make_packet(0);
    VLOG(INFO, "End Of File has been reached");
    sndpkt->hdr.data_size = 0;
    if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                ( const struct sockaddr *)&serveraddr, serverlen) < 0)
    {
        error("sendto");
    }
}
int send_packets(int curr, int last)
{
    if(curr > rsize) // if you got to the end of the file
    {
        end_packets();
    }
    if(curr>=last) // skip if out of sync, to receive
        return curr;
    if(curr<0) // handshake to send info about the file
    {
        sndpkt = make_packet(0);
        sndpkt->hdr.seqno = -1;
        sndpkt->hdr.ackno = rsize;
        sndpkt->hdr.ctr_flags = ACK;
        sndpkt->hdr.data_size = 0;
        if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                    ( const struct sockaddr *)&serveraddr, serverlen) < 0)
        {
            error("sendto");
        }
        rsend = -1;
        return 0;
    }
    else // if sending
    {
        long next;
        if(last >= rsize) // if the last one to be sent is past the final one, send them all and set it to last packet
            last = rsize;
        int rcurr = curr;
        curr = curr*DATA_SIZE;
        while(rcurr < last)
        {
            fseek(fp, curr, SEEK_SET);
            len = fread(buffer, 1, DATA_SIZE, fp);
            if (len <= 0)
            {
                // sndpkt = make_packet(0);
                // sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
                //         (const struct sockaddr *)&serveraddr, serverlen);
                return rcurr;
            }
            next = curr + len;
            sndpkt = make_packet(len);
            memcpy(sndpkt->data, buffer, len);
            sndpkt->hdr.seqno = curr;
            VLOG(DEBUG, "Sending packet %d to %s",
                    curr, inet_ntoa(serveraddr.sin_addr));
            /*
                * If the sendto is called for the first time, the system will
                * will assign a random port number so that server can send its
                * response to the src port.
                */
            if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                        ( const struct sockaddr *)&serveraddr, serverlen) < 0)
            {
                error("sendto");
            }
            curr = next;
            free(sndpkt);
            rcurr++;
        }
        int rnext = next/DATA_SIZE;
        return rnext;
    }
}

void resend_packets(int sig)
{
    ssthresh = (window_size/2)>2?(window_size/2):2;
    ss = 1;
    window_size = 1;
    lastav = rsend+window_size;
    if (sig == SIGALRM)
    {
        send_packets(rsend, lastav);
        VLOG(INFO, "Timout happend");
    }
    else
    {
        send_packets(rsend, lastav);
        VLOG(INFO, "3 dupe ACKs");
    }
}

void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}


void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}


/*
 * init_timer: Initialize timeer
 * delay: delay in milli seconds
 * sig_handler: signal handler function for resending unacknoledge packets
 */
void init_timer(int delay, void (*sig_handler)(int))
{
    signal(SIGALRM, resend_packets);
    timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    timer.it_value.tv_sec = delay / 1000;       // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}


int main (int argc, char **argv)
{
    int portno;
    char *hostname;
    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "r");
    if (fp == NULL) {
        error(argv[3]);
    }
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp); // check full size of the FILE
    rsize = (size + DATA_SIZE - 1) / DATA_SIZE; //size adjusted to DATA_SIZE chunks
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    lastav = window_size;
    /* initialize server server details */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
        fprintf(stderr,"ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    // Go Back N protocol
    init_timer(RETRY, resend_packets);



    while (looper)
    {
        lastsend = send_packets(lastsend, lastav);
        start_timer();
        //ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
        //struct sockaddr *src_addr, socklen_t *addrlen);
        if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
                    (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
        {
            error("recvfrom");
        }
        recvpkt = (tcp_packet *)buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        printf("%ld %d %d\n",send_base,recvpkt->hdr.seqno,recvpkt->hdr.ackno);
        if(send_base <= recvpkt->hdr.seqno) // if new ACK
        {
            stop_timer();
            if(!ss)
            {
                cc++;
            }
            else if(window_size>ssthresh)
                ss = 0;
            if(ss || cc>=window_size)
            {
                window_size++;
                cc = 0;
            }
        }
        if(send_base == recvpkt->hdr.ackno)
            dupe++;
        else
            dupe = 0;
        send_base = recvpkt->hdr.ackno;
        rsend = send_base/DATA_SIZE;
        lastav = rsend + window_size;
        if(dupe >= 3)
        {
            resend_packets(0);
        }
        if(rsend > lastsend) lastsend = rsend;
        printf("ACKd %d/%d\n", rsend, rsize);
        if((!rsize && !rsend) || rsend >= rsize)
        {
            end_packets();
            return 0;
        }
        if(!looper)
            return 0;
    }
    return 0;
}
