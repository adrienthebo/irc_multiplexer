/* irc_message.h
 *
 * Implementation for functions that operate on irc messages
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "irc_message.h"
#include "utilities.h"

/**
 * http://www.ietf.org/rfc/rfc1459.txt
 * 2.3.1 Message format in 'pseudo' BNF
 * 
 *    The protocol messages must be extracted from the contiguous stream of
 *    octets.  The current solution is to designate two characters, CR and
 *    LF, as message separators.   Empty  messages  are  silently  ignored,
 *    which permits  use  of  the  sequence  CR-LF  between  messages
 *    without extra problems.
 * 
 *    The extracted message is parsed into the components <prefix>,
 *    <command> and list of parameters matched either by <middle> or
 *    <trailing> components.
 * 
 *    The BNF representation for this is:
 * 
 * 
 * <message>  ::= [':' <prefix> <SPACE> ] <command> <params> <crlf>
 * <prefix>   ::= <servername> | <nick> [ '!' <user> ] [ '@' <host> ]
 * <command>  ::= <letter> { <letter> } | <number> <number> <number>
 * <SPACE>    ::= ' ' { ' ' }
 * <params>   ::= <SPACE> [ ':' <trailing> | <middle> <params> ]
 * 
 * <middle>   ::= <Any *non-empty* sequence of octets not including SPACE
 *                or NUL or CR or LF, the first of which may not be ':'>
 * <trailing> ::= <Any, possibly *empty*, sequence of octets not including
 *                  NUL or CR or LF>
 * 
 * <crlf>     ::= CR LF
 */
irc_message * parse_message(char *str) {

    irc_message *this = malloc(sizeof(irc_message)); 
    this->prefix = NULL;
    this->command = NULL;
    this->params = NULL;

    //TODO Fix all the glaring issues with strlen
    char *head = str;
    char *tail = head;

    //Attempt to extract prefix
    if(*tail == ':') {
	tail++;
	while(*tail != ' ') {
	    tail++;
	}

	this->prefix = malloc(sizeof(char) * (tail - head) + 1);
	memset(this->prefix, 0, (tail - head) + 1);
	strncpy(this->prefix, head, tail - head);

	while(*tail == ' ') tail++;
	head = tail;
    }

    //Extract command 

    //Check to see if we're receiving a server reply
    if(isdigit(head[0]) && isdigit(head[1]) && isdigit(head[2])) {
	#ifdef DEBUG
	fprintf(stderr, "Received server reply!\n");
	#endif /* DEBUG */
    }

    while(*tail != ' ') {
	tail++;
    }
    this->command = malloc(sizeof(char) * (tail - head) + 1);
    memset(this->command, 0, (tail - head) + 1);
    strncpy(this->command, head, tail - head);

    //Spool past whitespace
    while(*tail == ' ') {
	tail++;
    }

    //Attempt to extract params
    int trailing = 0;
    head = tail;

    char *buf[256];
    memset(buf, 0, 256);
    char **ptr = buf;

    //Loop over the string until we see any situation indicating a line end
    while(*tail != '\r' && *tail != '\n' && *tail != '\0') {

	//We tokenize on whitespace
	if(!trailing && *tail == ' ') {
	    char *tmp = malloc(sizeof(char) * (tail - head + 1));
	    memset(tmp, 0, (tail - head) + 1);
	    strncpy(tmp, head, tail - head);

	    fprintf(stdout, "Token: %s\n", tmp);
	    
	    *ptr++ = tmp;
	    head = tail;
	    head++;
	}

	//If we discover a space followed by a colon, then we've hit the trailing
	if(!trailing && *tail == ' ' && *(tail + 1) == ':') {
	    puts("Encountered <trailing>\n");
	    trailing = 1;
	    head++;
	}
	tail++;
    }

    //TODO remove
    this->params = malloc(sizeof(char) * (tail - head) + 1);
    memset(this->params, 0, (tail - head) + 1);
    strncpy(this->params, head, tail - head);

    *ptr++ = this->params;

    //Store all the parsed params into an array
    this->params_array = malloc(sizeof(char *) * (ptr - buf));
    memcpy(this->params_array, buf, (ptr - buf));

    #ifdef DEBUG
    fprintf(stdout, "Size of params: %ld\n", ptr - buf);
    fprintf(stdout, "this->prefix: %s\n", this->prefix);
    fprintf(stdout, "this->command: %s\n", this->command);
    for(int i = 0; i < (ptr - buf); i++) {
	printf("%d: \"%s\"\n", i, buf[i]);
    }
    //fprintf(stdout, "this->params: %s\n", this->params);
    #endif /* DEBUG */

    return this;
}

