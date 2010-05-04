/* irc_multiplexer.c
 *
 * Implements an irc multiplexer
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>
#include "irc_multiplexer.h"

int get_irc_socket(irc_multiplexer *this, const char *server_name, in_port_t server_port) {

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

    freeaddrinfo(query_result);
    puts("Connected\n");
    return sock;
}

int get_listen_socket(irc_multiplexer *this, char *socket_path) {

    //Prep sockaddr struct
    struct sockaddr_un unix_socket;
    strcpy(unix_socket.sun_path, socket_path);
    unix_socket.sun_family = AF_UNIX;

    //Establish and bind socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sock < 0) {
	perror("socket()");
	exit(1);
    }
    else {
	printf("socket fd: %d\n", sock);
    }
    if(bind(sock, (struct sockaddr *) &unix_socket, sizeof(unix_socket)) != 0) {
	perror("bind()");
	fprintf(stdout, "errno: %d, EINVAL: %d\n", errno, EINVAL);
	fprintf(stdout, "unix_socket.sun_family: %d, AF_UNIX: %d\n", 
		unix_socket.sun_family, AF_UNIX);
	exit(1);
    }
    return sock;
}

int process(irc_multiplexer *this) {

    int irc_socket = get_irc_socket(this, "irc.cat.pdx.edu", 6667);
    int listen_socket = get_listen_socket(this, "/tmp/ircbot.sock");

    //Get recv buffer size
    unsigned int rcvbuf;
    unsigned int rcvbuf_len = sizeof(rcvbuf);
    getsockopt(irc_socket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &rcvbuf_len);
    char buf[rcvbuf];
    printf("Socket buffer size: %d\n", rcvbuf);

    fd_set readfds;
    while(1) {
	//Initialize read fd_set
	FD_ZERO(&readfds);
	FD_SET(irc_socket, &readfds);
	FD_SET(listen_socket, &readfds);

	//Zero out buffer
	memset(buf, 0, rcvbuf);

	//Initialize timeout
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	if(select(irc_socket + 1, &readfds, NULL, NULL, &timeout) == 0) {
	    fputc('.', stdout);
	    fflush(stdout);
	}
	else if(FD_ISSET(irc_socket, &readfds)) {
	    recv(irc_socket, buf, rcvbuf_len - 1, 0);
	    fputs(buf, stdout);
	}
	else if(FD_ISSET(listen_socket, &readfds)) {
	    fputs("BAZINGA!", stdout);
	}
    }
    
    return 0;
}

