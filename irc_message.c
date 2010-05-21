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

/* parse_prefix
 *
 * Attempts to parse the prefix from an irc message, and store it in 
 * irc_message->prefix
 */
char * parse_prefix(irc_message *this, char *msg) {
    
    //return as-is if no leading colon
    if(*msg != ':') {
	#ifdef DEBUG
	fprintf(stderr, "No prefix.\n");
	#endif /* DEBUG */
	return msg;
    }

    char *tail = msg;

    while(*tail != ' ') {
	tail++;
    }

    size_t len = tail - msg;

    this->prefix = malloc(len);
    strncpy(this->prefix, msg, len - 1);
    *(this->prefix + len) = '\0';

    return tail;
}

/* parse_command
 *
 * Extracts the command from an IRC message, and stores it in 
 * this->command
 */
char * parse_command(irc_message *this, char *msg) {

    char *tail = msg;

    //Check to see if we're receiving a server reply
    if(isdigit(msg[0]) && isdigit(msg[1]) && isdigit(msg[2])) {
	#ifdef DEBUG
	fprintf(stderr, "Received server reply.\n");
	#endif /* DEBUG */
    }

    while(*tail != ' ') {
	tail++;
    }

    size_t len = tail - msg;

    this->command = malloc(len + 1);
    strncpy(this->command, msg, len);
    *(this->command + len + 1) = '\0';

    return tail;
}

void add_param(irc_message *this, char *param) {

    //Create new array of char *'s, and then add our new param to it
    if(this->params_array == NULL) {
	this->params_array = malloc(sizeof(char *));
	(this->params_array)[0] = param;
	this->params_len++;
    }
    //Expand array and add new param
    else {
	char ** new_params = malloc(sizeof(char *) * (this->params_len + 1));
	char ** old_params = this->params_array;

	memset(new_params, 0, this->params_len + 1);
	memcpy(new_params, old_params, (sizeof(char *) *this->params_len));

	new_params[this->params_len] = param;
	this->params_len++;

	this->params_array = new_params;
	free(old_params);
    }
}

/* parse_params
 *
 * Recursively extracts params from an IRC message 
 */
char * parse_params(irc_message *this, char *msg) {

    if(*msg != ' ') {
	fprintf(stderr, "ERROR: malformed params field\n");
	return NULL;
    }

    char *lookahead = msg;
    while(*lookahead == ' ') {
	lookahead++;
    }

    if(strcmp(msg, "\r\n") == 0) {
	return NULL;
    }

    //Check to see if we're about to read trailing
    if(strncmp(msg, " :", 2) == 0) {
	
	msg += 2;
	char *tail = msg;

	while(strncmp(tail, "\r\n", 2) != 0) {
	    tail++;
	}
	
	size_t len = tail - msg;
	char *substr = malloc(len + 1);
	strncpy(substr, msg, len);
	*(substr + len) = '\0';

	add_param(this, substr);
	return NULL;
    }
    //We're still reading space delimited params
    else {
	msg++;
	char *tail = msg;

	while(*tail != ' ' && strncmp(tail, "\r\n", 2) != 0) {
	    tail++;
	}

	size_t len = tail - msg;
	char *substr = malloc(len + 1);
	strncpy(substr, msg, len);
	*(substr + len) = '\0';

	add_param(this, substr);
	return parse_params(this, tail);
    }
}

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
irc_message * parse_message(char *msg) {

    irc_message *this = malloc(sizeof(irc_message)); 
    this->prefix = NULL;
    this->command = NULL;
    this->params = NULL;
    this->params_array = NULL;
    this->params_len = 0;

    msg = parse_prefix(this, msg);
    while(*msg == ' ') msg++;
    msg = parse_command(this, msg);
    parse_params(this, msg);

    #ifdef DEBUG
    fprintf(stderr, "this->prefix: \"%s\"\n", this->prefix);
    fprintf(stderr, "this->command: \"%s\"\n", this->command);
    fprintf(stderr, "Size of params: %ld\n", this->params_len);
    for(int i = 0; i < this->params_len; i++) {
	printf("%d: \"%s\"\n", i, (this->params_array)[i]);
    }
    fprintf(stderr, "----------------------------------------\n");
    #endif /* DEBUG */

    return this;
}
