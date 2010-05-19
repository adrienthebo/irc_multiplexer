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

/*
 * Null out values that should be null, so we don't get madness and anarchy
 * when uninitialized strings are used.
 */
void init_multiplexer(irc_multiplexer *this) {
    this->line_buffer = NULL;
    this->client_sockets = NULL;
    this->on_connect = 0;
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

    //Query DNS for hostname
    int error = getaddrinfo(server_name, NULL, NULL, &query_result);
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
	exit(1);
    }

    this->listen_socket = sock;
    //Listen for lots and lots of connections, and accept them ALL.
    listen(sock, 100000);
}

/* 
 * Accepts a connection on the AF_UNIX socket
 */
void accept_client_socket(irc_multiplexer *this) {
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

/**
 * http://www.ietf.org/rfc/rfc1459.txt
 * 2.3.1 Message format in 'pseudo' BNF
 * 
 *    The protocol messages must be extracted from the contiguous stream of
 *    octets.  The current solution is to designate two characters, CR and
 *    LF, as message separators.   Empty  messages  are  silently  ignored,
 *    which permits  use  of  the  sequence  CR-LF  between  messages
 *    without extra problems.
 * 
 *    The extracted message is parsed into the components <prefix>,
 *    <command> and list of parameters matched either by <middle> or
 *    <trailing> components.
 * 
 *    The BNF representation for this is:
 * 
 * 
 * <message>  ::= [':' <prefix> <SPACE> ] <command> <params> <crlf>
 * <prefix>   ::= <servername> | <nick> [ '!' <user> ] [ '@' <host> ]
 * <command>  ::= <letter> { <letter> } | <number> <number> <number>
 * <SPACE>    ::= ' ' { ' ' }
 * <params>   ::= <SPACE> [ ':' <trailing> | <middle> <params> ]
 * 
 * <middle>   ::= <Any *non-empty* sequence of octets not including SPACE
 *                or NUL or CR or LF, the first of which may not be ':'>
 * <trailing> ::= <Any, possibly *empty*, sequence of octets not including
 *                  NUL or CR or LF>
 * 
 * <crlf>     ::= CR LF
 */
irc_message * parse_message(char *str) {

    irc_message *new_msg = malloc(sizeof(irc_message)); 
    new_msg->prefix = NULL;
    new_msg->command = NULL;
    new_msg->params = NULL;

    //TODO Fix all the glaring issues with strlen
    char *head = str;
    char *tail = head;

    //Attempt to extract prefix
    if(*tail == ':') {
	tail++;
	while(*tail != ' ') {
	    tail++;
	}

	new_msg->prefix = malloc(sizeof(char) * (tail - head) + 1);
	memset(new_msg->prefix, 0, (tail - head) + 1);
	strncpy(new_msg->prefix, head, tail - head);

	while(*tail == ' ') tail++;
	head = tail;
    }

    //Extract command 

    //Check to see if we're receiving a server reply
    if(isdigit(head[0]) && isdigit(head[1]) && isdigit(head[2])) {
	fprintf(stderr, "Received server reply!\n");
    }

    while(*tail != ' ') {
	tail++;
    }
    new_msg->command = malloc(sizeof(char) * (tail - head) + 1);
    memset(new_msg->command, 0, (tail - head) + 1);
    strncpy(new_msg->command, head, tail - head);

    //Attempt to extract params
    tail++;
    head = tail;
    while(*tail != '\r' && *tail != '\n' && *tail != '\0') {
	tail++;
    }
    new_msg->params = malloc(sizeof(char) * (tail - head) + 1);
    memset(new_msg->params, 0, (tail - head) + 1);
    strncpy(new_msg->params, head, tail - head);
    #ifdef DEBUG
    fprintf(stdout, "new_msg->prefix: %s\n", new_msg->prefix);
    fprintf(stdout, "new_msg->command: %s\n", new_msg->command);
    fprintf(stdout, "new_msg->params: %s\n", new_msg->params);
    #endif /* DEBUG */

    return new_msg;
}

/*
 * Sends an entire line of data, and handles partial sends.
 * TODO rework this so it uses the main select loop
 */
void send_server(irc_multiplexer *this, char *msg) {
    unsigned int payload_len = strlen(msg);

    while(payload_len > 0) {
	#ifdef DEBUG
	fprintf(stderr, "Attempting to send msg \"%s\" with payload_len %u\n", msg, payload_len);
	#endif /* DEBUG */
	ssize_t sent_data = send(this->server_socket, msg, payload_len, 0);
	msg += sent_data;
	payload_len -= sent_data;

	#ifdef DEBUG
	fprintf(stderr, "Sent %lu bytes, remaining msg is now \"%s\", payload_len now %u\n", 
		(unsigned long)sent_data, msg, payload_len);
	#endif /* DEBUG */
    }
}

void connection_manager(irc_multiplexer *this, irc_message *msg) {
    if(strcmp(msg->command, "NOTICE") == 0) {
	fprintf(stderr, "Received a notice. Ignoring.\n");
    }
    else if(strcmp(msg->command, "PING") == 0) {
	char buf[256];
	snprintf(buf, 256, "PONG :%s\r\n", msg->params);
	send_server(this, buf);
    }
}

/*
 * Handles incoming message fragments received from the server socket.
 * Buffers input until a newline is reached, then acts as necessary.
 */
void read_server_socket(irc_multiplexer *this, char *msg_fragment) {
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

	strn_append(&(this->line_buffer), msg_fragment, newline_pos + 1);

	#ifdef DEBUG
	fprintf(stdout, "Received line ");
	#endif /* DEBUG */
	fputs(this->line_buffer, stdout);
	irc_message *msg = parse_message(this->line_buffer);
	connection_manager(this, msg);
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
	//We're still aggregating information into the line_buffer
	str_append(&(this->line_buffer), msg_fragment);
    }
}

void set_nick(irc_multiplexer *this) {

    char buf[256];
    memset(buf, 0, 256);
    snprintf(buf, 256, "NICK %s\r\n", this->identity.nick);
    send_server(this, buf);
}

void register_user(irc_multiplexer *this) {
    char buf[256];

    //Parameters: <username> <hostname> <servername> <realname>
    snprintf(buf, 256, "USER %s %s %s %s\r\n", 
	    this->identity.username, this->identity.hostname,
	    this->identity.servername, this->identity.realname);

    send_server(this, buf);
}

int prep_select(irc_multiplexer *this, fd_set *readfds) {
    FD_ZERO(readfds);
    FD_SET(this->server_socket, readfds);
    FD_SET(this->listen_socket, readfds);

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
	FD_SET(current->fd, readfds);
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

	if(this->on_connect == 0) {
	    register_user(this);
	    set_nick(this);

	    char buf[256];
	    snprintf(buf, 256, "MODE %s B\r\n", this->identity.nick);
	    send_server(this, buf);
	    this->on_connect = 1;
	}

	//Begin main execution
	int ready_fds = select(nfds + 1, &readfds, NULL, NULL, &(this->timeout));

	//If no input, print a dot to indicate inactivity.
	if(ready_fds == 0) {
	    fputc('.', stdout);
	    fflush(stdout);
	}
	//If server_socket is set, then handle that input
	else if(FD_ISSET(this->server_socket, &readfds)) {
	    recv(this->server_socket, buf, this->rcvbuf_len - 1, 0);
	    read_server_socket(this, buf);
	}
	//We received a new connection from a client locally
	else if(FD_ISSET(this->listen_socket, &readfds)) {
	    fputs("Received client connection on local socket", stdout);
	    accept_client_socket(this);
	}
	// else one of our clients sent a message
	//TODO forward message from client to server
    }
}

