#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

int main(int argc, char **argv) {

    char *server_name = "irc.cat.pdx.edu";
    in_port_t server_port = 6667;

    //Info for resolving DNS address
    struct addrinfo *query_result;
    struct sockaddr_in *sockdata;
    int error;

    //Query DNS for hostname
    error = getaddrinfo(server_name, NULL, NULL, &query_result);
    if(error != 0) {
	fprintf(stderr, "Error resolving %s: %s\n", server_name, gai_strerror(error));
	exit(1);
    }

    //Prep socket info
    sockdata = (struct sockaddr_in *)query_result->ai_addr;
    //memcpy(sockdata, query_result->ai_addr, sizeof(struct sockaddr));
    sockdata->sin_port = htons(server_port);

    //freeaddrinfo(query_result);

    //Generate socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock < 0) {
	perror("socket()");
	exit(1);
    }

    //Establish connection
    if(connect(sock, (struct sockaddr *)sockdata, sizeof(*sockdata)) != 0 ) {
	perror("connect()");
	exit(1);
    }
    puts("Connected\n");
   
    //Get recv buffer size
    unsigned int rcvbuf;
    unsigned int rcvbuf_len = sizeof(rcvbuf);
    getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &rcvbuf_len);
    char buf[rcvbuf];
    printf("Socket buffer size: %d\n", rcvbuf);

    fd_set readfds;
    while(1) {
	//Initialize read fd_set
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);

	//Zero out buffer
	memset(buf, 0, rcvbuf);

	//Initialize timeout
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	if(select(sock + 1, &readfds, NULL, NULL, &timeout) == 0) {
	    fputc('.', stdout);
	    fflush(stdout);
	}
	else if(FD_ISSET(sock, &readfds)) {
	    recv(sock, buf, rcvbuf_len - 1, 0);
	    fputs(buf, stdout);
	}
    }
    
    return 0;
}

