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

void init_multiplexer(irc_multiplexer *this) {
    this->line_buffer = NULL;
    this->client_sockets = NULL;
}

void set_irc_server(irc_multiplexer *this, char *server_name, in_port_t server_port) {

    this->server = server_name;
    this->port = server_port;

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
    sockdata->sin_port = htons(server_port);

    //Generate socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock < 0) {
	perror("socket()");
	exit(1);
    }

    //Establish connection to IRC server
    if(connect(sock, (struct sockaddr *)sockdata, sizeof(*sockdata)) != 0 ) {
	perror("connect()");
	exit(1);
    }

    freeaddrinfo(query_result);

    /* We need to retrieve the size of the socket buffer so that we can 
     * allocate enough space for our local buffer. Retrieving less than the
     * entire buffer will truncate the message, and that is baaaad.
     */
    this->server_socket = sock;
    this->rcvbuf_len = sizeof(this->rcvbuf);
    getsockopt(this->server_socket, SOL_SOCKET, SO_RCVBUF, 
	    &(this->rcvbuf), &(this->rcvbuf_len));

    #ifdef DEBUG
    fprintf(stderr, "Connected to %s:%d\n", this->server, this->port);
    fprintf(stderr, "rcbuf_len: %d\n", this->rcvbuf_len);
    fprintf(stderr, "rcbuf: %u\n", this->rcvbuf_len);
    #endif /* DEBUG */

}

void set_local_socket(irc_multiplexer *this, char *socket_path) {

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
	#ifdef DEBUG
	fprintf(stdout, "errno: %d, EINVAL: %d\n", errno, EINVAL);
	fprintf(stdout, "listen_socket.sun_family: %d, AF_UNIX: %d\n", 
		listen_socket.sun_family, AF_UNIX);
	#endif /* DEBUG */
	exit(1);
    }

    this->listen_socket = sock;
    //Listen for lots and lots of connections, and accept them ALL.
    listen(sock, 100000);
}

void accept_socket(irc_multiplexer *this) {
    if(this->client_sockets == NULL) {
	this->client_sockets = malloc(sizeof(client_socket));
    }
    else {
	client_socket *new_socket = malloc(sizeof(client_socket));
	new_socket->next = this->client_sockets;
	this->client_sockets = new_socket;
    }

    this->client_sockets->fd = accept(this->listen_socket, NULL, NULL);
}

void handle_incoming_message(irc_multiplexer *this, char *msg_fragment) {
    int has_newline = 0;

    /* Scan fragment for a newline, to determine whether we can pipe our
     * buffer to clients.
     */
    for(int i = 0; i <= strlen(msg_fragment); i++) {
	if(msg_fragment[i] == '\n') {
	    has_newline++;
	}
    }

    /* We have no idea what recv is going to pass us, but we operate on 
     * newline terminated strings. So, we need to concatenate them, and 
     * then when we hit a newline we ship the line off.
     */
    if(this->line_buffer == NULL) {
	/* No previously stored line, initialize a persistent buffer and 
	 * store the line there
	 */
	this->line_buffer = malloc(strlen(msg_fragment) + 1);
	memset(this->line_buffer, 0, strlen(msg_fragment) + 1);
	
	strcpy(this->line_buffer, msg_fragment);
    }
    else if(has_newline) {
	/* Current message fragment has a newline, we need to append up to 
	 * the newline and then send it off.
	 */

	#ifdef DEBUG
	fprintf(stderr, "Newline found, fragment: \"%s\"\n", msg_fragment);
	#endif /* DEBUG */

	char *new_line_buffer = malloc(strlen(msg_fragment) + 
		strlen(this->line_buffer) + 1);
	memset(new_line_buffer, 0, strlen(msg_fragment) + 
		strlen(this->line_buffer) + 1);

	strcat(new_line_buffer, this->line_buffer);
	strcat(new_line_buffer, msg_fragment);

	//TODO fix memory leak
	//free(this->line_buffer);
	this->line_buffer = new_line_buffer;
	fputs(this->line_buffer, stdout);
	#ifdef DEBUG
	fprintf(stdout, "Detected newline!\n");
	#endif /* DEBUG */
	free(this->line_buffer);
	this->line_buffer = NULL;
    }
    else {
	/* We're still aggregating information into the line_buffer
	 */

	//Allocate new buffer that has room and zero it out
	char *new_line_buffer = malloc(strlen(msg_fragment) + 
		strlen(this->line_buffer) + 1);
	memset(new_line_buffer, 0, strlen(msg_fragment) + 
		strlen(this->line_buffer) + 1);

	strcat(new_line_buffer, this->line_buffer);
	strcat(new_line_buffer, msg_fragment);

	free(this->line_buffer);
	this->line_buffer = new_line_buffer;

	#ifdef DEBUG
	fprintf(stderr, "%s\n", this->line_buffer);
	#endif /* DEBUG */
    }

    //TODO forward message to all listening clients
}

void start_server(irc_multiplexer *this) {
    char buf[this->rcvbuf];
    while(1) {
	//Prep recv buffer
	memset(buf, 0, this->rcvbuf);

	//Initialize read fd_set
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(this->server_socket, &readfds);
	FD_SET(this->listen_socket, &readfds);

	//Find the highest file descriptor for select
	int nfds = 0;
	if( nfds < this->server_socket) nfds = this->server_socket;
	if( nfds < this->listen_socket) nfds = this->listen_socket;

	//Load all the client sockets
	for(client_socket *current = this->client_sockets;
		current != NULL;
		current = current->next ) {

	    #ifdef DEBUG
	    fprintf(stderr, "client fd: %d\n", current->fd);
	    #endif /* DEBUG */

	    //Locate highest fd for select()
	    if(nfds < current->fd) nfds = current->fd;
	    //Add client fd to listen sockets
	    FD_SET(current->fd, &readfds);
	}

	//Initialize timeout
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	//Begin main execution
	int ready_fds = select(nfds + 1, &readfds, NULL, NULL, &timeout);
	if(ready_fds == 0) {
	    fputc('.', stdout);
	    fflush(stdout);
	}
	else if(FD_ISSET(this->server_socket, &readfds)) {
	    recv(this->server_socket, buf, this->rcvbuf_len - 1, 0);
	    handle_incoming_message(this, buf);
	}
	else if(FD_ISSET(this->listen_socket, &readfds)) {
	    fputs("Received client connection on local socket", stdout);
	    accept_socket(this);
	}
	// else one of our clients sent a message
	//TODO forward message from client to server
    }
}

