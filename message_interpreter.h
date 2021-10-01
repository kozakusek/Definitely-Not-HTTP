#ifndef SERVER_HTTP_MESSAGE_INTERPRETER_H
#define SERVER_HTTP_MESSAGE_INTERPRETER_H

#include <exception>
#include <string>
#include <unordered_map>
#include <iostream>

// Taking into account the maximum length of a path, possible
// values of <method> and of course the forum - we set this value to 2^13.
constexpr int max_line_len = 1 << 13;

// An empty exception for ending the connection.
struct close_exc : public std::exception {
};

// Struct with important data for manage_messages.
struct msg_data {
    std::string act_line;               // The current line to interpret.
    std::string path;                   // The requested resource.
    bool isget{};
    bool flag_501{};                    // HTTP method out of our scope.
    bool flag_404{};                    // The file would not be found anyway.
    unsigned int code{};                // HTTP code to be sent.
    std::string reason_phrase;
    std::string headers[3];             // 0 -- Connection; 1  -- Content-Length; 3 -- Location.
    int msg_sock{};
    std::string dir;                    // The directory path with server's resources.

    msg_data() = default;

    msg_data(int msg_sock, std::string &dir);
};

// Main logic of intepreting the http messages sent to our server.
// More in message_interpreter.cpp.
void manage_messages(std::istream &is, int msg_sock, std::unordered_map<std::string,
                     std::string> &map, std::string &dir);

#endif //SERVER_HTTP_MESSAGE_INTERPRETER_H
