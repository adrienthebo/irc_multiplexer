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
#include <errno.h>
#include <ctype.h>

#include "irc_multiplexer.h"
#include "utilities.h"

/* Internal function declarations */
void on_remote_read(char * msg_str, void *args);
void on_client_read(char * msg_str, void *args);
void accept_client_socket(irc_multiplexer *this);
void connection_manager(irc_multiplexer *this, irc_message *msg);
void set_nick(irc_multiplexer *this);
void register_user(irc_multiplexer *this);
int prep_select(irc_multiplexer *this, fd_set *readfds);
void check_select(irc_multiplexer *this, fd_set *readfds);

/* 
 * Callback method for the buffered socket that wraps the remote connection
 */
void on_remote_read(char * msg_str, void *args) {
    //Unpack args
    irc_multiplexer *this = (irc_multiplexer *) args;
    irc_message *irc_msg = parse_message(msg_str);

    //Run internal checks on the message to see if we need to react
    connection_manager(this, irc_msg);

    //Forward message to all clients
    for(client_socket *current = this->clients;
	    current != NULL;
	    current = current->next ) {

	fprintf(stdout, "Sending message to client fd %d\n", current->bufsock->fd);
	current->bufsock->write_buffer = msg_str;
	write_buffered_socket(current->bufsock);
    }

    destroy_message(irc_msg);
}

void on_client_read(char * msg_str, void *args) {

    fprintf(stdout, "Received message \"%s\" from client\n", msg_str);
}


/*
 * Constructor for an irc_multiplexer
 * Null out values that should be null, so we don't get madness and anarchy
 * when uninitialized strings are used.
 */
void init_multiplexer(irc_multiplexer *this) {
    this->line_buffer = NULL;
    this->clients = NULL;
    this->on_connect = 0;
    this->remote = new_buffered_socket("\r\n", &on_remote_read, this);

}

/*
 * Generates an AF_INET socket to the remote and stores it
 */
void set_irc_server(irc_multiplexer *this, char *server_name, in_port_t server_port) {

    this->server = server_name;
    this->port = server_port;

    //Info for resolving DNS address
    struct addrinfo *query_result;

    #ifdef DEBUG
    fprintf(stderr, "Resolving %s... ", server_name);
    #endif /* DEBUG */

    //Query DNS for hostname
    int error = getaddrinfo(server_name, NULL, NULL, &query_result);
    if(error != 0) {
	fprintf(stderr, "Error resolving %s: %s\n", server_name, gai_strerror(error));
	exit(1);
    }

    #ifdef DEBUG
    fprintf(stderr, "%s\n", inet_ntoa(((struct sockaddr_in *)query_result->ai_addr)->sin_addr));
    #endif /* DEBUG */

    //Prep socket info
    struct sockaddr_in *sockdata = (struct sockaddr_in *)query_result->ai_addr;
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
    this->remote->fd = sock;
    this->rcvbuf_len = sizeof(this->rcvbuf);
    getsockopt(this->remote->fd, SOL_SOCKET, SO_RCVBUF, 
	    &(this->rcvbuf), &(this->rcvbuf_len));

    #ifdef DEBUG
    fprintf(stderr, "Connected to %s:%d\n", this->server, this->port);
    fprintf(stderr, "rcvbuf_len: %d\n", this->rcvbuf_len);
    fprintf(stderr, "rcvbuf: %u\n", this->rcvbuf_len);
    #endif /* DEBUG */

}

/* 
 * Generates and listens for clients on the listen socket
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
	exit(1);
    }

    this->listen_socket = sock;
    //Listen for lots and lots of connections, and accept them ALL.
    listen(sock, 100000);
}

/* 
 * Accepts a connection on the local listen socket
 */
void accept_client_socket(irc_multiplexer *this) {
    if(this->clients == NULL) {
	this->clients = malloc(sizeof(client_socket));
	this->clients->next = NULL;
    }
    else {
	client_socket *new_socket = malloc(sizeof(client_socket));
	new_socket->next = this->clients;
	this->clients = new_socket;
    }
    this->clients->bufsock = new_buffered_socket("\r\n", on_client_read, NULL);
    this->clients->bufsock->fd = accept(this->listen_socket, NULL, NULL);
}

void connection_manager(irc_multiplexer *this, irc_message *msg) {
    if(strcmp(msg->command, "NOTICE") == 0) {
	#ifdef DEBUG
	//fprintf(stderr, "Ignoring notice.\n");
	#endif /* DEBUG */
    }
    else if(strcmp(msg->command, "PING") == 0) {
	char buf[256];
	snprintf(buf, 256, "PONG :%s\r\n", (msg->params_array)[0]);
	this->remote->write_buffer = buf;
	write_buffered_socket(this->remote);
    }
}

void set_nick(irc_multiplexer *this) {

    char buf[256];
    memset(buf, 0, 256);
    snprintf(buf, 256, "NICK %s\r\n", this->identity.nick);
    this->remote->write_buffer = buf;
    write_buffered_socket(this->remote);
}

void register_user(irc_multiplexer *this) {
    char buf[256];

    //Parameters: <username> <hostname> <servername> <realname>
    snprintf(buf, 256, "USER %s %s %s %s\r\n", 
	    this->identity.username, this->identity.hostname,
	    this->identity.servername, this->identity.realname);

    this->remote->write_buffer = buf;
    write_buffered_socket(this->remote);
}

int prep_select(irc_multiplexer *this, fd_set *readfds) {
    FD_ZERO(readfds);
    FD_SET(this->remote->fd, readfds);
    FD_SET(this->listen_socket, readfds);

    //Find the highest file descriptor for select
    int nfds = 0;
    if( nfds < this->remote->fd) nfds = this->remote->fd;
    if( nfds < this->listen_socket) nfds = this->listen_socket;

    //Load all the client sockets
    for(client_socket *current = this->clients;
	    current != NULL;
	    current = current->next ) {

	#ifdef DEBUG
	//fprintf(stderr, "adding client fd to select(): %d\n", current->bufsock->fd);
	#endif /* DEBUG */

	//Locate highest fd for select()
	if(nfds < current->bufsock->fd) nfds = current->bufsock->fd;
	//Add client fd to listen sockets
	FD_SET(current->bufsock->fd, readfds);
    }

    return nfds;
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
	int nfds = prep_select(this, &readfds);

	//Initialize timeout
	this->timeout.tv_sec = 1;
	this->timeout.tv_usec = 0;

	/* On connect setup and such
	 * TODO rethink and generalize this.
	 */
	if(this->on_connect == 0) {
	    register_user(this);
	    set_nick(this);

	    //TODO put mode changes in a function
	    char buf[256];
	    snprintf(buf, 256, "MODE %s B\r\n", this->identity.nick);
	    this->remote->write_buffer = buf;
	    write_buffered_socket(this->remote);
	    this->on_connect = 1;
	}

	//Begin main execution
	int ready_fds = select(nfds + 1, &readfds, NULL, NULL, &(this->timeout));

	//If no input, print a dot to indicate inactivity.
	if(ready_fds == 0) {
	    fputc('.', stdout);
	    fflush(stdout);
	}
	//If remote->fd is set, then handle that input
	else if(FD_ISSET(this->remote->fd, &readfds)) {
	    read_buffered_socket(this->remote);
	    //recv(this->remote->fd, buf, this->rcvbuf_len - 1, 0);
	    //read_server_socket(this->remote, buf);
	}
	//We received a new connection from a client locally
	else if(FD_ISSET(this->listen_socket, &readfds)) {
	    fputs("Received client connection on local socket", stdout);
	    accept_client_socket(this);
	}
	else {
	// else one of our clients sent a message
	//TODO forward message from client to server
	    for(client_socket *current = this->clients;
		    current != NULL;
		    current = current->next ) {

		if(FD_ISSET(current->bufsock->fd, &readfds)) {
		    fprintf(stderr, "received message from client fd: %d\n", current->bufsock->fd);
		    int client_rcvbuf;
		    unsigned int client_rcvbuf_len = sizeof(client_rcvbuf);
		    getsockopt(current->bufsock->fd, SOL_SOCKET, SO_RCVBUF, &client_rcvbuf, &client_rcvbuf_len);

		    char clientbuf[client_rcvbuf + 1];
		    memset(clientbuf, 0, client_rcvbuf + 1);
		    size_t received = recv(current->bufsock->fd, clientbuf, client_rcvbuf, 0);
		    fprintf(stderr, "message was \"%s\" of size %lu\n", clientbuf, received);
		    fprintf(stderr, "----------------------------------------\n");
		}
	    }
	}
    }
}

