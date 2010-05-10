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

/*
 * Null out values that should be null, so we don't get madness and anarchy
 * when uninitialized strings are used.
 */
void init_multiplexer(irc_multiplexer *this) {
    this->line_buffer = NULL;
    this->client_sockets = NULL;
}

/*
 * Generates an AF_INET socket to the server and stores it
 */
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

/* 
 * Generates and listens to a AF_UNIX socket
 */
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
	fprintf(stderr, "errno: %d, EINVAL: %d\n", errno, EINVAL);
	fprintf(stderr, "listen_socket.sun_family: %d, AF_UNIX: %d\n", 
		listen_socket.sun_family, AF_UNIX);
	#endif /* DEBUG */
	exit(1);
    }

    this->listen_socket = sock;
    //Listen for lots and lots of connections, and accept them ALL.
    listen(sock, 100000);
}

/* 
 * Accepts a connection on the AF_UNIX socket
 */
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

/* 
 * Allocates memory for the concatenation of two strings, frees the old
 * string, and replaces the old string with the new string.
 *
 * TODO revisit this and think about how badly this would explode if a
 * static string was inputted.
 */
void strn_append(char **old_str, char *append_str, size_t n) {

    int new_str_len = strlen(*old_str) + n;
    char * new_str = malloc(new_str_len + 1);
    memset(new_str, 0, new_str_len + 1);

    strcat(new_str, *old_str);
    strncat(new_str, append_str, n);

    free(*old_str);
    *old_str = new_str;
}

/* 
 * Calls strn_append to append the entire append_str.
 */
void str_append(char **old_str, char *append_str) {
    strn_append(old_str, append_str, strlen(append_str));
}

/*
 * Handles incoming message fragments received from the server socket.
 * Concatenates and appends when necessary, and should send complete
 * strings off to listening sockets.
 */
void handle_incoming_message(irc_multiplexer *this, char *msg_fragment) {
    int newline_pos = -1;

    /* Scan fragment for a newline, to determine whether we can pipe our
     * buffer to clients.
     */
    for(int i = 0; i <= strlen(msg_fragment); i++) {
	if(msg_fragment[i] == '\n') {
	    newline_pos = i;
	    break;
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
    else if(newline_pos != -1) {
	/* Current message fragment has a newline, we need to append up to 
	 * the newline and then send it off.
	 */

	#ifdef DEBUG
	fprintf(stderr, "Newline found, fragment: \"%s\"\n", msg_fragment);
	#endif /* DEBUG */

	strn_append(&(this->line_buffer), msg_fragment, newline_pos + 1);

	#ifdef DEBUG
	fprintf(stdout, "Received line ");
	#endif /* DEBUG */
	fputs(this->line_buffer, stdout);
	//TODO do something useful with a complete line

	int excess_str = strlen(msg_fragment) - newline_pos;
	if(excess_str > 0) {
	    this->line_buffer = malloc(excess_str + 1);
	    memset(this->line_buffer, 0, excess_str + 1);
	    strcpy(this->line_buffer, msg_fragment + newline_pos + 1);
	}
	else {
	    this->line_buffer = NULL;
	}
    }
    else {
	/* We're still aggregating information into the line_buffer
	 */

	str_append(&(this->line_buffer), msg_fragment);

	#ifdef DEBUG
	fprintf(stderr, "line_buffer: %s\n", this->line_buffer);
	#endif /* DEBUG */
    }
}

/* 
 * Kicks off a while loop to handle all input from all sockets in every
 * direction, evar.
 */
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
	this->timeout.tv_sec = 1;
	this->timeout.tv_usec = 0;

	//Begin main execution
	int ready_fds = select(nfds + 1, &readfds, NULL, NULL, &(this->timeout));
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

