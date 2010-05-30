/* irc_message.h
 *
 * Defines information for storing and manipulating irc messages.
 */

#ifndef _IRC_MESSAGE_H
#define _IRC_MESSAGE_H

typedef struct irc_message_struct {
    char *msg;
    char *prefix;
    char *command;
    char **params_array;
    size_t params_len;
} irc_message;

irc_message * parse_message(char *str);

void destroy_message(irc_message *this);

#endif /* _IRC_MESSAGE_H */
