/* buffered_socket.h
 *
 * Defines a buffered socket.
 */

#ifndef BUFFERED_SOCKET_H
#define BUFFERED_SOCKET_H

typedef struct buffered_socket_struct {
    int fd;

    char *delimiter;

    char *read_buffer;
    char *write_buffer;

    void (*read_callback)(char *, void *);
    void *read_callback_args;
} buffered_socket;

buffered_socket * new_buffered_socket(char *delimiter, void (*read_callback)(char *, void *), void *read_callback_args);

/*
 * Reads from a buffered socket, and triggers the callback if the line 
 * terminator was reached.
 *
 * Returns 0 if read was successful, 1 if read was successful and 
 * buffer was flushed, and -1 on error.
 */
int read_buffered_socket(buffered_socket *this);

/*
 * Writes from buffer into a buffered socket
 *
 * Returns 0 if write was successful, 1 if write was successful and 
 * buffer was completely flushed, and -1 on error.
 */
int write_buffered_socket(buffered_socket *this);

/*
 * Checks to see if a buffered socket is still connected
 *
 * Returns 1 on connected, 0 on disconnected
 */
int bufsock_is_connected(buffered_socket *this);

#endif /* BUFFERED_SOCKET_H */
