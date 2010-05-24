/* client.c
 *
 * Implements a simple listener client for the multiplexer.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char **argv) {
    
    struct sockaddr_un unix_socket;
    memset(&unix_socket, 0, sizeof(unix_socket));
    int fd;
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) {
	perror("connect()");
	exit(1);
    }

    unix_socket.sun_family = AF_UNIX;
    strcpy(unix_socket.sun_path, "/tmp/ircbot.sock");

    if(connect(fd, (struct sockaddr *) &unix_socket, 
		SUN_LEN(&unix_socket)) != 0 ) {
	perror("connect()");
	exit(1);
    }
    else {
	fprintf(stdout, "Connected!\n");
    }

    unsigned int rcvbuf = 0;
    unsigned int rcvbuf_len = sizeof(rcvbuf);

    getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &rcvbuf_len);
    fprintf(stderr, "Buffer size: %u\n", rcvbuf);

    char buf[rcvbuf + 1];
    buf[rcvbuf + 1] = '\0';

    while(1) {
	memset(buf, 0, rcvbuf + 1);
	ssize_t retrieved = recv(fd, buf, rcvbuf, 0);
	signed int so_error = 0;
	unsigned int so_error_len = sizeof(so_error);

	getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &so_error_len);

	if(so_error != 0) {
	    fprintf(stdout, "Socket connection closed\n");
	    exit(1);
	}
	
	else if(retrieved > rcvbuf) {
	    fprintf(stderr, "ERROR: Data truncation occurred!\n");
	}

	fprintf(stdout, "Message received: \n\"%s\"\n---\n", buf);

	memset(buf, 0, rcvbuf + 1);
	strcpy(buf, "foo\r\n");

	ssize_t sent = send(fd, buf, strlen(buf), 0);

	if(sent != strlen(buf)) {
	    fprintf(stderr, "ERROR: dur wut\n");
	}
    }
    fprintf(stdout, "Exiting.\n");
    return 0;
}
