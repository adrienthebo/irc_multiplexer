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
    
    fprintf(stdout, "msg: \"%s\"\n", msg);
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
    fprintf(stderr, "Adding new param %p \"%s\"\n", param, param);
    fprintf(stderr, "this->params_len: %lu\n", this->params_len);
    //Create new array of char *'s, and then add our new param to it
    if(this->params_array == NULL) {
	fprintf(stderr, "No existing params array, creating one.\n");
	this->params_array = malloc(sizeof(char *));
	(this->params_array)[0] = param;
	this->params_len++;
    }
    //Expand array and add new param
    else {
	fprintf(stderr, "Appending char ptr \"%s\" to existing array.\n", param);
	char ** new_params = malloc(sizeof(char *) * (this->params_len + 1));
	char ** old_params = this->params_array;

	memset(new_params, 0, this->params_len + 1);
	memcpy(new_params, old_params, (sizeof(char *) *this->params_len));

	new_params[this->params_len] = param;
	this->params_len++;

	this->params_array = new_params;
	//free(old_params);
    }
    fprintf(stderr, "this->params_len: %lu\n", this->params_len);

    for(int i = 0; i < this->params_len; i++) {
	char ** arr = this->params_array;
	fprintf(stderr, "addressof params_array[%d] = %p\n", i, arr[i]);
	fprintf(stderr, "params_array[%d] = %s\n", i, arr[i]);
    }
}

/* parse_params
 *
 * Recursively extracts params from an IRC message 
 */
char * parse_params(irc_message *this, char *msg) {

    fprintf(stderr, "Parsing message %s\n", msg);

    if(*msg != ' ') {
	fprintf(stderr, "ERROR: malformed params field\n");
	return NULL;
    }

    char *lookahead = msg;
    while(*lookahead == ' ') {
	fprintf(stderr, "lookahead: '%c'\n", *lookahead);
	lookahead++;
    }
    fprintf(stderr, "terminal lookahead: '%c'\n", *lookahead);

    if(strcmp(msg, "\r\n") == 0) {
	fprintf(stdout, "Hit end of msg without <trailing> param component\n");
	return NULL;
    }

    //Check to see if we're about to read trailing
    if(strncmp(msg, " :", 2) == 0) {
	fprintf(stdout, "Hit <trailing> param component\n");
	
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

	fprintf(stderr, "BLort -> ");
	while(*tail != ' ' && strncmp(tail, "\r\n", 2) != 0) {
	    fputc(*tail, stderr);
	    tail++;
	}
	fputs("\n", stderr);

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

    //TODO Fix all the glaring issues with strlen
    char *head = msg;
    char *tail = head;

    msg = parse_prefix(this, msg);
    msg++;
    msg = parse_command(this, msg);

    parse_params(this, msg);

#ifdef FUCK
    tail = head = msg;

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

	//If we discover a space followed by a colon, then we've hit the trailing
	if(!trailing && *tail == ' ' && *(tail + 1) == ':') {
	    fprintf(stderr, "Trailing: \"%s\"\n", tail);
	    trailing = 1;
	}

	//We tokenize on whitespace
	if(trailing != 2 && *tail == ' ' && head != tail) {
	    char *tmp = malloc(sizeof(char) * (tail - head + 1));
	    memset(tmp, 0, (tail - head) + 1);
	    strncpy(tmp, head, tail - head);

	    *ptr++ = tmp;
	    head = tail;
	    head++;

	    /* We have to tokenize all strings delimited by whitespace, but
	     * when we hit the colon we consider everything following to be
	     * a single token.
	     */
	    if(trailing == 1) {
		//Increment pointers past the colon
		head++, tail++;
		trailing = 2;
	    }
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

#endif /* FUCK */

    #ifdef DEBUG
    fprintf(stdout, "this->prefix: \"%s\"\n", this->prefix);
    fprintf(stdout, "this->command: \"%s\"\n", this->command);
    fprintf(stdout, "Size of params: %ld\n", this->params_len);
    for(int i = 0; i < this->params_len; i++) {
	printf("%d: \"%s\"\n", i, (this->params_array)[i]);
    }
    #endif /* DEBUG */

    return this;
}
