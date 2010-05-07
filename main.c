/*
 * Gimpy lil test harness for the multiplexer
 */
#include "irc_multiplexer.h"

int main(int argc, char **argv) {

    irc_multiplexer catirc;
    
    set_irc_server(&catirc, "irc.cat.pdx.edu", 6667);
    set_local_socket(&catirc, "/tmp/ircbot.sock");

    catirc.nick = "finchbot";
    catirc.username = "finch";
    catirc.realname = "finchbot";
    catirc.hostname = "finch@localhost";
    catirc.servername = "*";
    catirc.client_sockets = NULL;

    start_server(&catirc);
    return 0;
}

