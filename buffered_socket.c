/* buffered_socket.c
 *
 * Implements a buffered socket 
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>

#include "utilities.h"
#include "buffered_socket.h"

buffered_socket * new_buffered_socket(char *delimiter, void (*read_callback)(char *), void (*write_callback)(char *)) {

    buffered_socket *this = malloc(sizeof(buffered_socket));

    this->delimiter = delimiter;

    this->read_buffer = NULL;
    this->write_buffer = NULL;

    this->read_callback = read_callback;
    this->write_callback = write_callback;

    return this;
}

/*
 * Handles incoming message fragments received from the server socket.
 * Buffers input until a newline is reached, then acts as necessary.
 *
 * TODO fix handling of delimiter inside first message
 */
int read_buffered_socket(buffered_socket *this) {

    //Create a buffer that is the max size of a socket read
    int rcvbuf = 0;
    unsigned int rcvbuf_len = sizeof(int);
    getsockopt(this->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &rcvbuf_len);
    char buf[rcvbuf + 1];
    memset(buf, 0, rcvbuf + 1);

    #ifdef DEBUG
    fprintf(stderr, "rcvbuf_len: %d\n", rcvbuf_len);
    fprintf(stderr, "rcvbuf: %u\n", rcvbuf);
    #endif /* DEBUG */

    //TODO compare bytes read vs bytes available
    recv(this->fd, buf, rcvbuf, 0);

    /* Scan fragment for a newline, to determine whether we can pipe our
     * buffer to clients.
     */
    char *delimiter_ptr = strstr(buf, this->delimiter);
    size_t delimiter_offset = delimiter_ptr - buf;

    /* We have no idea what recv is going to pass us, but we operate on 
     * the delimiter given to us. We store our input into a buffer until we
     * hit that delimiter and then we ship the buffer off.
     */
    if(this->read_buffer == NULL) {
	/* No previous buffer, initialize a persistent buffer and 
	 * store the line there
	 */
	this->read_buffer = malloc(strlen(buf) + 1);
	memset(this->read_buffer, 0, strlen(buf) + 1);
	
	strcpy(this->read_buffer, buf);
	return 0;
    }
    else if(delimiter_ptr != NULL) {
	// Current message contains the delimiter

	//Append up to the delimiter
	strn_append(&(this->read_buffer), buf, delimiter_offset - 1);

	//Fire off the callback!
	(*(this->read_callback))(this->read_buffer);

	free(this->read_buffer);

	//If anything remains after the delimiter, add it to a new buffer

	size_t excess_str = strlen(delimiter_ptr + strlen(this->delimiter));
	if(excess_str > 0) {
	    this->read_buffer = malloc(excess_str + 1);
	    memset(this->read_buffer, 0, excess_str + 1);
	    strcpy(this->read_buffer, delimiter_ptr + strlen(this->delimiter));
	}
	else {
	    this->read_buffer = NULL;
	}
	return 1;
    }
    else {
	//We're still aggregating information into the read_buffer
	str_append(&(this->read_buffer), buf);
	return 0;
    }
}

int write_buffered_socket(buffered_socket *this) {

    char *msg = this->write_buffer;
    unsigned int payload_len = strlen(msg);

    #ifdef DEBUG
    fprintf(stderr, "Attempting to send msg \"%s\" with payload_len %u\n", msg, payload_len);
    #endif /* DEBUG */
    ssize_t sent_data = send(this->fd, msg, payload_len, 0);
    msg += sent_data;
    payload_len -= sent_data;

    #ifdef DEBUG
    fprintf(stderr, "Sent %lu bytes, remaining msg is now \"%s\", payload_len now %u\n", (unsigned long)sent_data, msg, payload_len);
    #endif /* DEBUG */

    if(payload_len == 0) {
	free(this->write_buffer);
	return 1;
    }
    else {
	char *new_write_buffer = malloc(strlen(msg) + 1);
	strcpy(new_write_buffer, msg);
	
	free(this->write_buffer);
	this->write_buffer = new_write_buffer;
	return 0;
    }
}

