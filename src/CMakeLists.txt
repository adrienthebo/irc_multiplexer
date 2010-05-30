include_directories(.)
add_definitions(-Wall -DDEBUG -std=gnu99)
add_executable( bot server.c irc_multiplexer.c irc_multiplexer.h
    irc_message.c irc_message.h buffered_socket.c buffered_socket.h
    utilities.h utilities.c)
add_executable( client client.c)
