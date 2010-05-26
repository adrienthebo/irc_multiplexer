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

buffered_socket * new_buffered_socket(char *delimiter, void (*read_callback)(char *, void *), void *read_callback_args) {

    buffered_socket *this = malloc(sizeof(buffered_socket));

    this->fd = -1;

    this->delimiter = delimiter;

    this->read_buffer = NULL;
    this->write_buffer = NULL;

    this->read_callback = read_callback;
    this->read_callback_args = read_callback_args;
    return this;
}

int manage_read_buffer(buffered_socket *this, char *buf) {

    // Check to see if message contains delimiter
    char *delimiter_ptr = strstr(buf, this->delimiter);

    /* We have no idea what recv is going to pass us, but we operate on 
     * the delimiter given to us. We store our input into a buffer until we
     * hit that delimiter and then we ship the buffer off.
     */
    if(delimiter_ptr != NULL) {
	// Current message contains the delimiter

	size_t delimiter_offset = delimiter_ptr - buf;
	size_t substr_len = delimiter_offset + strlen(this->delimiter);

	if(this->read_buffer == NULL) {
	    // No previous buffer, initialize one
	    this->read_buffer = malloc(substr_len + 1);
	    memset(this->read_buffer, 0, substr_len + 1);
	    strncpy(this->read_buffer, buf, substr_len);
	}

	//Append up to the delimiter if it isn't the first character
	if(substr_len + 1 > 0) {
	    char *old = strn_append(&(this->read_buffer), buf, substr_len + 1);
	    free(old);
	}

	//Fire off the callback!
	(*(this->read_callback))(this->read_buffer, this->read_callback_args);

	free(this->read_buffer);

	//If anything remains after the delimiter, add it to a new buffer
	size_t excess = strlen(delimiter_ptr + strlen(this->delimiter));
	char *excess_str = delimiter_ptr + strlen(this->delimiter);
	if(excess > 0) {

	    //Check to see if there's another delimiter in our excess string
	    delimiter_ptr = strstr(excess_str, this->delimiter);
	    if(delimiter_ptr != NULL) {
		manage_read_buffer(this, excess_str);
	    }
	    else {
		this->read_buffer = malloc(excess + 1);
		memset(this->read_buffer, 0, excess + 1);
		strcpy(this->read_buffer, excess_str);
	    }

	}
	else {
	    this->read_buffer = NULL;
	}
	return 1;
    }
    else {
	//We're still aggregating information into the read_buffer
	char *old = str_append(&(this->read_buffer), buf);
	free(old);
	return 0;
    }
}

/*
 * Handles incoming message fragments received from the server socket.
 * Buffers input until a newline is reached, then acts as necessary.
 */
int read_buffered_socket(buffered_socket *this) {

    //Create a buffer that is the max size of a socket read
    int rcvbuf = 8;
    unsigned int rcvbuf_len = sizeof(rcvbuf);

    /*
    getsockopt(this->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &rcvbuf_len);
    */
    int error = setsockopt(this->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, rcvbuf_len);
    if(error != 0) {
	perror("setsockopt()");
	return -1;
    }

    char buf[rcvbuf + 1];
    memset(buf, 0, rcvbuf + 1);

    //Receive data and ensure data truncation didn't occur
    ssize_t received = recv(this->fd, buf, rcvbuf, 0);
    if(received > rcvbuf) {
	fprintf(stderr, "Error: data truncated during recv().\n");
    }

    return manage_read_buffer(this, buf);
}

/*
 * Writes the current write buffer to a socket
 *
 * TODO
 *  - Make this non-blocking
 *  - Either truncate message to fit buffer or return error
 */
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
	this->write_buffer = NULL;
	return 1;
    }
    else {
	this->write_buffer = msg;
	return 0;
    }
}

