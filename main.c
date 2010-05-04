/*
 * Gimpy lil test harness for the multiplexer
 */
#include "irc_multiplexer.h"

int main(int argc, char **argv) {

    irc_multiplexer catirc;
    catirc.server = "irc.cat.pdx.edu";
    catirc.port = 6667;
    
    catirc.server_socket = get_irc_socket(&catirc, "irc.cat.pdx.edu", 6667);
    catirc.unix_socket = get_listen_socket(&catirc, "/tmp/ircbot.sock");

    process(&catirc);

    return 0;
}

