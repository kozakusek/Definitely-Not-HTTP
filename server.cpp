#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ext/stdio_filebuf.h>
#include <unistd.h>
#include <unordered_map>
#include <filesystem>
#include "message_interpreter.h"

using namespace std;

constexpr int queue_length = 10;           // TCP queue.
int port_num = 8080;                       // We set the default to 8080.
unordered_map<string, string> servers_map; // A mapping for correlated servers' resources.
string dir;                                // Directory with server's resources.

// Interprets the program parameters and extracts the path to the file
// with correlated servers' resources to 'servers'.
void init_args(int argc, char *argv[], filesystem::path &servers) {
    filesystem::path files = argv[1];
    servers = argv[2];

    // argv[3] indicates the optional change of the connection port.
    if (argc == 4) {
        // Validate the numeric value of argv[3].
        if (string(argv[3]).find_first_not_of("0123456789") == string::npos) {
            port_num = stoi(argv[3]);
        } else {
            // An empty base exception for notifying the caller about lethal errors.
            throw exception();
        }
    }

    if (!(filesystem::is_directory(files) && filesystem::is_regular_file(servers))) {
        throw exception();
    }

    dir = files.string();
}

// Extracts the data from the file given in the program arguments,
// now located under 'servers'. Creates the mapping from resources
// to http urls as described in the task.
void load_servers(filesystem::path &servers) {
    fstream fstr(servers);
    string content, cont_key, cont_val;
    stringstream content_stream;
    getline(fstr, content);
    while (!fstr.eof()) {
        content_stream = stringstream(content);
        content_stream >> cont_key;
        content_stream >> content;
        cont_val = "http://" + content + ":";
        content_stream >> content;
        cont_val += content + cont_key;

        if (servers_map.find(cont_key) == servers_map.end()) {
            servers_map.insert({cont_key, cont_val});
        }

        getline(fstr, content);
    }

    fstr.close();
}

int main(int argc, char *argv[]) {
    if (argc < 3 || 4 < argc) {
        // Incorrect number of arguments.
        return EXIT_FAILURE;
    }
    filesystem::path servers;

    try {
        init_args(argc, argv, servers);
    } catch (exception &e) {
        // A lethal error occurred during the interpretation of the arguments.
        // For instance what should be a file is a directory and vice versa.
        return EXIT_FAILURE;
    }

    try {
        load_servers(servers);
    } catch (exception &e) {
        // A lethal error occurred during the interpretation of the file 'server'.
        return EXIT_FAILURE;
    }

    // Declarations of variables used for establishing and maintaining
    // the TCP connection.
    int sock, msg_sock;
    struct sockaddr_in server_address{};
    struct sockaddr_in client_address{};
    socklen_t client_address_len;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return EXIT_FAILURE;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port_num);

    if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        return EXIT_FAILURE;
    }

    if (listen(sock, queue_length) < 0) {
        return EXIT_FAILURE;
    }

    for (;;) {
        client_address_len = sizeof(client_address);

        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0) {
            return EXIT_FAILURE;
        }

        // Assigning an input stream for the msg_sock file descriptor.
        __gnu_cxx::stdio_filebuf<char> filebuf(msg_sock, ios::in);
        istream is(&filebuf);

        // Read and interpret the messages from the client and respond to them.
        // Ends connection by throwing close_exc.
        try {
            manage_messages(is, msg_sock, servers_map, dir);
        } catch (close_exc &e) {}

        if (close(msg_sock) < 0) {
            return EXIT_FAILURE;
        }
    }
}
