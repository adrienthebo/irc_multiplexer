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

#include "buffered_socket.h"
#include "irc_message.h"

typedef struct client_socket_struct {
    int fd;
    struct client_socket_struct *next;
} client_socket;

typedef struct irc_identity_struct {
    //Identity info
    char *nick;
    char *username;
    char *realname;
    char *hostname;
    char *servername;
} irc_identity;

typedef struct irc_multiplexer_struct {

    //Describe the server we're talking to
    char *server;
    in_port_t port;

    //IRC socket
    buffered_socket *remote;
    signed int rcvbuf;
    unsigned int rcvbuf_len;
    char *line_buffer;

    //Address for clients to connect to
    char *listen_socket_path;
    int listen_socket;
    client_socket *client_sockets;

    //select() timeout
    struct timeval timeout;

    irc_identity identity;
    int on_connect;

} irc_multiplexer;

void init_multiplexer(irc_multiplexer *this);
void set_irc_server(irc_multiplexer *this, char *server_name, in_port_t server_port);
void set_local_socket(irc_multiplexer *this, char *socket_path);
void start_server(irc_multiplexer *this);
#endif /* _IRC_MULTIPLEXER_H */

