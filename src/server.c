/*
 * Gimpy lil test harness for the multiplexer
 */
#include "irc_multiplexer.h"

int main(int argc, char **argv) {

    irc_multiplexer catirc;
    init_multiplexer(&catirc);
    set_irc_server(&catirc, "irc.cat.pdx.edu", 6667);
    set_local_socket(&catirc, "/tmp/ircbot.sock");

    catirc.identity.nick = "finchbot";
    catirc.identity.username = "finch";
    catirc.identity.realname = "finchbot";
    catirc.identity.hostname = "finch@localhost";
    catirc.identity.servername = "*";

    start_server(&catirc);
    return 0;
}

