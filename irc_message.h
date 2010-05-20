/* irc_message.h
 *
 * Defines information for storing and manipulating irc messages.
 */

#ifndef _IRC_MESSAGE_H
#define _IRC_MESSAGE_H

typedef struct irc_message_struct {
    char *prefix;
    char *command;
    char *params;
    char **params_array;
} irc_message;

irc_message * parse_message(char *str);

#endif /* _IRC_MESSAGE_H */
