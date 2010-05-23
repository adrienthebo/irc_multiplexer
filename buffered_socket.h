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

    void (*read_callback)(char *);
    void (*write_callback)(char *);
} buffered_socket;

buffered_socket * new_buffered_socket(char *delimiter, void (*read_callback)(char *), void (*write_callback)(char *));

/*
 * Reads into a buffered socket, and triggers the callback if the line 
 * terminator was reached.
 *
 * Returns 0 if read was successful, 1 if read was successful and 
 * buffer was flushed, and -1 on error.
 */
int read_buffered_socket(buffered_socket *this);

/*
 * Writes into a buffered socket, and triggers the callback if the line 
 * terminator was reached.
 *
 * Returns 0 if write was successful, 1 if write was successful and 
 * buffer was completely flushed, and -1 on error.
 */
int write_buffered_socket(buffered_socket *this);

#endif /* BUFFERED_SOCKET_H */
