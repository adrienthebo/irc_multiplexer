/* irc_multiplexer.h
 *
 * Defines the specs for an IRC multiplexer. It slices! It dices! It handles
 * multiple connections to a single irc connections, permitting asynchronous
 * communication!
 */

#ifndef _IRC_MULTIPLEXER_H
#define _IRC_MULTIPLEXER_H

#include <arpa/inet.h>
#include <glib.h>

typedef struct irc_multiplexer_struct {

    //Describe the server we're talking to
    char *server;
    in_port_t port;

    //Address for clients to connect to
    char *unix_socket_path;

    //fds to read from
    int server_socket;
    int unix_socket;
    GList *client_sockets;

    //Identity info
    char *nick;
    char *username;
    char *realname;
    char *hostname;
    char *servername;
} irc_multiplexer;

int set_irc_server(irc_multiplexer *mux, const char *server_name, in_port_t server_port);
int set_local_socket(irc_multiplexer *mux, char *socket_path);
int process(irc_multiplexer *mux);
#endif /* _IRC_MULTIPLEXER_H */

