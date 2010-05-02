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
    in_port_t server_port = 6667;

    //Info for resolving DNS address
    struct addrinfo *query_result;
    struct sockaddr_in *sockdata;
    int error;

    //Query DNS for hostname
    error = getaddrinfo(server_name, NULL, NULL, &query_result);

    if(error != 0) {
	fprintf(stderr, "Error resolving %s: %s\n", server_name, gai_strerror(error));
	exit(1);
    }

    sockdata = (struct sockaddr_in *)query_result->ai_addr;
    sockdata->sin_port = htons(server_port);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(sock < 0) {
	perror("socket()");
	exit(1);
    }

    //TODO clean this up
    int connect_retn = connect(sock, (struct sockaddr *)sockdata, sizeof(*sockdata));
    if(connect_retn != 0) {
	perror("connect()");
	exit(1);
    }
    puts("Connected\n");

    freeaddrinfo(query_result);
    return 0;
}

