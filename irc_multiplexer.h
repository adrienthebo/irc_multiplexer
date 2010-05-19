/* irc_multiplexer.h
 *
 * Defines the specs for an IRC multiplexer. It slices! It dices! It handles
 * multiple connections to a single irc connections, permitting asynchronous
 * communication!
 */

#ifndef _IRC_MULTIPLEXER_H
#define _IRC_MULTIPLEXER_H

#include <stdlib.h>
#include <arpa/inet.h>

typedef struct client_socket_struct {
    int fd;
    struct client_socket_struct *next;
} client_socket;

typedef struct irc_message_struct {
    char *prefix;
    char *command;
    char *params;
} irc_message;

typedef struct irc_multiplexer_struct {

    //Describe the server we're talking to
    char *server;
    in_port_t port;

    //Address for clients to connect to
    char *listen_socket_path;

    //select() timeout
    struct timeval timeout;

    //IRC socket
    int server_socket;
    signed int rcvbuf;
    unsigned int rcvbuf_len;
    char *line_buffer;

    int listen_socket;
    client_socket *client_sockets;

    //Identity info
    char *nick;
    char *username;
    char *realname;
    char *hostname;
    char *servername;
} irc_multiplexer;

void init_multiplexer(irc_multiplexer *this);
void set_irc_server(irc_multiplexer *this, char *server_name, in_port_t server_port);
void set_local_socket(irc_multiplexer *this, char *socket_path);
void start_server(irc_multiplexer *this);
#endif /* _IRC_MULTIPLEXER_H */

