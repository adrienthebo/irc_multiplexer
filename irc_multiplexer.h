/* irc_multiplexer.h
 *
 * Defines the specs for an IRC multiplexer. It slices! It dices! It handles
 * multiple connections to a single irc connections, permitting asynchronous
 * communication!
 */

#ifndef _IRC_MULTIPLEXER_H
#define _IRC_MULTIPLEXER_H

#include <glib.h>

typedef struct irc_multiplexer_struct {

    //Describe the server we're talking to
    char *irc_server;
    in_port_t irc_port;

    //Address for clients to connect to
    char *unix_socket_path;

    //fds to read from
    int server_socket;
    int unix_socket;
    GList *client_sockets;
} irc_multiplexer;

int get_irc_socket(irc_multiplexer *mux, const char *server_name, in_port_t server_port);
int get_listen_socket(irc_multiplexer *mux, char *socket_path);

#endif /* _IRC_MULTIPLEXER_H */

