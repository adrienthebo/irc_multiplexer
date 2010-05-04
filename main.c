/*
 * Gimpy lil test harness for the multiplexer
 */
#include "irc_multiplexer.h"

int main(int argc, char **argv) {

    irc_multiplexer catirc;
    catirc.server = "irc.cat.pdx.edu";
    catirc.port = 6667;
    
    set_irc_server(&catirc, "irc.cat.pdx.edu", 6667);
    set_local_socket(&catirc, "/tmp/ircbot.sock");

    process(&catirc);

    return 0;
}

