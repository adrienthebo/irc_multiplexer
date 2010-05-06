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
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include "irc_multiplexer.h"

int set_irc_server(irc_multiplexer *this, const char *server_name, in_port_t server_port) {

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
    puts("Connected");
    this->server_socket = sock;
    return sock;
}

int set_local_socket(irc_multiplexer *this, char *socket_path) {

    //Remove existing socket if it exists
    struct stat socket_stat;
    int error = stat(socket_path, &socket_stat);
    if(error == 0 && S_ISSOCK(socket_stat.st_mode)) {
#ifdef DEBUG
	fprintf(stderr, "Notice: Removing old socket %s\n", socket_path);
#endif
	unlink(socket_path);
    }
    else if(error == 0) {
	fprintf(stderr, "Error, %s already exists and is not a socket. Exiting.\n", socket_path);
	exit(1);
    }

    //Prep sockaddr struct
    struct sockaddr_un listen_socket;
    strcpy(listen_socket.sun_path, socket_path);
    listen_socket.sun_family = AF_UNIX;

    //Establish and bind socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sock < 0) {
	perror("socket()");
	exit(1);
    }

    if(bind(sock, (struct sockaddr *) &listen_socket, sizeof(listen_socket)) != 0) {
	perror("bind()");
	fprintf(stdout, "errno: %d, EINVAL: %d\n", errno, EINVAL);
	fprintf(stdout, "listen_socket.sun_family: %d, AF_UNIX: %d\n", 
		listen_socket.sun_family, AF_UNIX);
	exit(1);
    }
    this->listen_socket = sock;
    listen(sock, 100000);
    return sock;
}

int accept_socket(irc_multiplexer *this) {
    if(this->client_sockets == NULL) {
	this->client_sockets = malloc(sizeof(client_socket));
    }
    else {
	client_socket *new_socket = malloc(sizeof(client_socket));
	new_socket->next = this->client_sockets;
	this->client_sockets = new_socket;
    }

    this->client_sockets->fd = accept(this->listen_socket, NULL, NULL);
    return 0;
}

int process(irc_multiplexer *this) {

    /* We need to retrieve the size of the socket buffer so that we can 
     * allocate enough space for our local buffer. Retrieving less than the
     * entire buffer will truncate the message, and that is baaaad.
     */
    unsigned int rcvbuf;
    unsigned int rcvbuf_len = sizeof(rcvbuf);
    getsockopt(this->server_socket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &rcvbuf_len);
    
    //Prep recv buffer
    char buf[rcvbuf];
    memset(buf, 0, rcvbuf);

    //Initialize read fd_set
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(this->server_socket, &readfds);
    FD_SET(this->listen_socket, &readfds);

    //Find the highest file descriptor for select
    int nfds = 0;
    if( nfds < this->server_socket) nfds = this->server_socket;
    if( nfds < this->listen_socket) nfds = this->listen_socket;

    for(client_socket *current = this->client_sockets;
	    current != NULL;
	    current = current->next ) {
#ifdef DEBUG
	fprintf(stderr, "client fd: %d\n", current->fd);
#endif /* DEBUG */
	if(nfds < current->fd) nfds = current->fd;
	FD_SET(current->fd, &readfds);
    }

    //Initialize timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int ready_fds = select(nfds + 1, &readfds, NULL, NULL, &timeout);
    if(ready_fds == 0) {
	fputc('.', stdout);
	fflush(stdout);
    }
    else if(FD_ISSET(this->server_socket, &readfds)) {
	recv(this->server_socket, buf, rcvbuf_len - 1, 0);
	fputs(buf, stdout);
	//TODO forward message to all listening clients
    }
    else if(FD_ISSET(this->listen_socket, &readfds)) {
	fputs("Received client connection on local socket", stdout);
	accept_socket(this);
    }
    // else one of our clients sent a message
    return 0;
}
