/* irc_message.h
 *
 * Implementation for functions that operate on irc messages
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "irc_message.h"

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

    irc_message *new_msg = malloc(sizeof(irc_message)); 
    new_msg->prefix = NULL;
    new_msg->command = NULL;
    new_msg->params = NULL;

    //TODO Fix all the glaring issues with strlen
    char *head = str;
    char *tail = head;

    //Attempt to extract prefix
    if(*tail == ':') {
	tail++;
	while(*tail != ' ') {
	    tail++;
	}

	new_msg->prefix = malloc(sizeof(char) * (tail - head) + 1);
	memset(new_msg->prefix, 0, (tail - head) + 1);
	strncpy(new_msg->prefix, head, tail - head);

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
    new_msg->command = malloc(sizeof(char) * (tail - head) + 1);
    memset(new_msg->command, 0, (tail - head) + 1);
    strncpy(new_msg->command, head, tail - head);

    //Attempt to extract params
    tail++;
    head = tail;
    while(*tail != '\r' && *tail != '\n' && *tail != '\0') {
	tail++;
    }
    new_msg->params = malloc(sizeof(char) * (tail - head) + 1);
    memset(new_msg->params, 0, (tail - head) + 1);
    strncpy(new_msg->params, head, tail - head);
    #ifdef DEBUG
    fprintf(stdout, "new_msg->prefix: %s\n", new_msg->prefix);
    fprintf(stdout, "new_msg->command: %s\n", new_msg->command);
    fprintf(stdout, "new_msg->params: %s\n", new_msg->params);
    #endif /* DEBUG */

    return new_msg;
}
