#include "message_interpreter.h"
#include <regex>
#include <unistd.h>
#include <unordered_map>
#include <fstream>
#include <filesystem>
using namespace std;

msg_data::msg_data(int msg_sock, std::string &dir) : path(4096, '\0'), msg_sock(msg_sock), dir(dir) {}

// Throwable object for managing closing HTTP codes.
struct server_exception : public std::exception {
    std::string msg;
    unsigned int code;
    server_exception(unsigned int code, std::string &&s) : msg(s), code(code) {}
};

// Takes msg_data struct and interprets the current line.
// Throws server exception if the request line is incorrect.
void request_line(msg_data &data) {
    regex r(R"(([a-zA-Z0-9!#$%&'\*\+\-\.^_`\|~]+) ([a-zA-Z0-9\.\-\/]+) HTTP/1.1\r)");
    smatch sm;

    if (!regex_match(data.act_line, sm, r)) {
        throw server_exception(400, "Wrong request");
    }
    
    if (sm[1].str() == "GET") {
        data.isget = true;
        if (sm[2].str()[0] != '/') {
            throw server_exception(400, "Wrong path");
        }
    } else if (sm[1].str() == "HEAD") {
        data.isget = false;
        if (sm[2].str()[0] != '/') {
            throw server_exception(400, "Wrong path");
        }
    } else {
        data.isget = false;
        data.flag_501 = true;
    }

    // Save the path to data. The length limit is 4096 hence longer paths
    // will not be found anyway and therefore there is no need to waste more
    // space for them.
    if (sm[2].length() > 4096) {
        data.flag_404 = true;
    } else {
        data.path = sm[2];
    }
}

// Interprets the <header-field> under data.act_line.
// Throws server_exception if the format is incorrect or
// repetitions of not ignored <header-name>s occur.
void header_line(msg_data &data) {
    // From the forum we assume <header-name>: "[a-zA-Z0-9_-]+".
    // We make the ".*" lazy in <header-value> to transfer all leading and tracing " ".
    regex r(R"(([a-zA-Z0-9_-]+): *(.*?) *\r)");
    smatch sm;

    if (!regex_match(data.act_line, sm, r)) {
        throw server_exception(400, "Header format error");
    }

    string key(sm[1]);
    string val(sm[2]);

    // Transforms the <header-name> to lower case to enable
    // non-case-sensitive comparison.
    transform(key.begin(), key.end(), key.begin(),
              [](unsigned char c) { return tolower(c); });

    // According to the forum we can ignore all <header-name>s other than:
    // -- connection
    // -- content-length
    // and for those ignored we do not verify if any repetitions occur.
    if (key == "connection") {
        if (!data.headers[0].empty()) {
            throw server_exception(400, "Header repetition \"" + key + "\"");
        }
        data.headers[0] = val;
    } else if (key == "content-length") {
        if (!data.headers[1].empty()) {
            throw server_exception(400, "Header repetition \"" + key + "\"");
        }

        // Check whether <content-val> was a numeric value.
        if (val.find_first_not_of("0123456789") != string::npos) {
            throw server_exception(400, "Wrong parameter");
        }

        data.headers[1] = val;
        // According to the forum we can interpret every message with body as incorrect.
        if (!val.empty() && stoi(val) > 0) {
            throw server_exception(400, "Messege body not allowed");
        }
    }
}

// Writes the status line to file descriptor data.msg_sock using
// code and reason-pharse that are encoded in data.
void status_line(msg_data &data) {
    string sl("HTTP/1.1 " + to_string(data.code) + " " + data.reason_phrase + "\r\n");
    write(data.msg_sock, sl.c_str(), sl.length());
}

// Writes the header lines to file descriptor data.msg_sock using
// data.headers. The only important ones for our server are:
// -- connection: close
// -- Location: <location>
void server_header_lines(msg_data &data) {
    if (data.headers[0] == "close") {
        write(data.msg_sock, "Connection: close\r\n", 19);
    }

    if (!data.headers[2].empty()) {
        string msg = "Location: " + data.headers[2] + "\r\n";
        write(data.msg_sock, msg.c_str(), msg.length());
    }
    
    if (data.code != 200)
        write(data.msg_sock, "\r\n", 2);
}

// Writes bytes from fs to data.msg_sock using buffering.
// Determines the size of file (as far is I know - prone to mistakes
// if the file is being edited during that short period of time).
// Manages also:
// -- content-type
// -- content-length.
// Closes the fs!!!
void body_lines(msg_data &data, fstream &fs, filesystem::path &file) {
    char buff[1000];

    write(data.msg_sock, "Content-Type: application/octet-stream\r\n", 40);

    string clen = "Content-Length: " + to_string(filesystem::file_size(file)) + + "\r\n\r\n";
    write(data.msg_sock, clen.c_str(), clen.length());

    if (data.isget) {
      while (!fs.eof()) {
            fs.read(buff, 1000);
            write(data.msg_sock, buff, fs.gcount());
        }
    }
    fs.close();
}

// Checks whether the path dir//path made from data fields is
// "inside" the data.dir directory.
bool is_filepath_legal(msg_data &data) {
    filesystem::path dirpath(data.dir);
    filesystem::path filepath(data.dir + data.path);
    filesystem::path relative = filepath.lexically_normal().lexically_relative(dirpath.lexically_normal());

    // After transformation and relativiastion the "incorrect" path will start with "..".
    return !(relative.string()[0] == '.' && relative.string()[1] == '.');
}

// Reads up to max_line_len bytes from is to str until it
// encounters '\n' or '\0' (they do not edn up in str).
void m_getline(istream &is, string &str) {
    char buff[max_line_len + 1];
    is.getline(buff, max_line_len);
    buff[is.gcount()] = '\0';
    str = string(buff);
}

// Main logic of intepreting the http messages sent to our server.
// Takes as arguments:
// is       -- input stream to read data recieved from the client,
// msg_sock -- file descriptor to send data to the client,
// map      -- mapping of rescources to http urls,
// dir      -- the directory in which servers resoures are lockated.
// Throws close_exc if the connection is to be closed.
void manage_messages(istream &is, int msg_sock, unordered_map<string, string> &map, std::string &dir) {
    msg_data data;
    is.exceptions(ifstream::failbit | ifstream::badbit);

    for (;;) {
        try {
            // Reset all the data.
            data = msg_data(msg_sock, dir);

            m_getline(is, data.act_line);
            if (is.eof()) {
                throw server_exception(400, "Request missing");
            }
            request_line(data);

            m_getline(is, data.act_line);
            if (is.eof()) {
                throw server_exception(400, "CRLF missing");
            }
            while (data.act_line.length() > 1) {
                header_line(data);
                m_getline(is, data.act_line);
                if (is.eof()) {
                    throw server_exception(400, "CRLF missing");
                }
            }
            if (data.act_line.length() == 0 || data.act_line[0] != '\r') {
                throw server_exception(400, "CRLF missing");
            }

            filesystem::path filepath(dir + data.path);
            fstream fs;

            if (!(data.flag_404 || data.flag_501)) {
                if (is_filepath_legal(data) && filesystem::is_regular_file(filepath)) {
                    fs.open(filepath);
                    if (fs.is_open()) {
                        data.code = 200;
                        data.reason_phrase = "OK";
                    } else {
                        // Empty base exceptions are used to indicate server errors that
                        // should end the connection and send the code 500 to the client.
                        throw exception();
                    }
                } else if (map.find(data.path) != map.end()) {
                    data.code = 302;
                    data.reason_phrase = "Found on remote";
                    data.headers[2] = map.find(data.path)->second;
                } else {
                      data.code = 404;
                      data.reason_phrase = "Could not find the requested file";
                }
            } else {
                data.code = data.flag_404 ? 404 : data.flag_501 ? 501 : 200;
                data.reason_phrase = data.flag_404 ? "Could not find the requested file"
                                   : data.flag_501 ? "Unknown method" : "OK";
            }

            status_line(data);
            server_header_lines(data);
            if (data.code == 200) {
                body_lines(data, fs, filepath);
            }

            if (data.headers[0] == "close") {
                throw close_exc();
            }
        } catch (server_exception &e) {
            data.code = e.code;
            data.reason_phrase = e.msg;
            status_line(data);

            data.headers[0] = "close";
            server_header_lines(data);

            throw close_exc();
        } catch (close_exc &e) {
            throw e;
        } catch (exception &e) {
            data.code = 500;
            data.reason_phrase = "Server Error";
            status_line(data);
            data.headers[0] = "close";
            server_header_lines(data);
            throw close_exc();
        }
    }
}
