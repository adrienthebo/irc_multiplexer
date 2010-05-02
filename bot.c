#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char **argv) {

    char *server_name = "irc.cat.pdx.edu";
    //in_port_t server_port = 6667;

    //Info for resolving DNS address
    struct addrinfo *result;
    struct addrinfo *result_iter;
    int error;

    //Query DNS for hostname
    error = getaddrinfo(server_name, NULL, NULL, &result);

    if(error != 0) {
	fprintf(stderr, "Error resolving %s: %s\n", server_name, gai_strerror(error));
	exit(1);
    }

    //Iterate over all returned addresses
    for(result_iter = result; 
	    result_iter != NULL; 
	    result_iter = result_iter->ai_next) {

	struct sockaddr *current = result_iter->ai_addr;

	switch(current->sa_family) {
	    case AF_INET:
		fprintf(stdout, "Received inet sockaddr\n");
		struct sockaddr_in *curr_inet = (struct sockaddr_in *)current;
		fprintf(stdout, "Resolved address: %s\n",
			inet_ntoa(curr_inet->sin_addr));
		break;
	    case AF_INET6:
		fprintf(stdout, "Received inet6 sockaddr\n");
		break;
	    default:
		fprintf(stdout, "Unknown sa_family: %d\n",current->sa_family);
	}
    }

    freeaddrinfo(result);
    return 0;
}

