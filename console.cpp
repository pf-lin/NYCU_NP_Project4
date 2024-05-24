#include <boost/algorithm/string.hpp> // Include the header file for boost::split
#include <boost/asio.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

using boost::asio::ip::tcp;
using namespace std;

#define MAX_CONNECTION 5

const string contentType = "Content-Type: text/html\r\n\r\n";
const string contentHead = R"(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>NP Project 3 Console</title>
    <link
      rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
        font-size: 1rem !important;
      }
      body {
        background-color: #232731;
      }
      pre {
        color: #D8DEE9;
      }
      b {
        color: #a3be8c;
      }
      th {
        color: #81A1C1;
      }
    </style>
  </head>
)";
const string contentBodyFront = R"(
  <body>
    <table class="table table-dark table-bordered">
      <thead>
        <tr>
)";
const string contentBodyMiddle = R"(
        </tr>
      </thead>
      <tbody>
        <tr>
)";
const string contentBodyEnd = R"(
        <tr>
      </tbody>
    </table>
  </body>
</html>
)";

struct ConnectionInfo {
    string host = "";
    string port = "";
    string file = "";
};

vector<ConnectionInfo> connections(MAX_CONNECTION);

class Client : public std::enable_shared_from_this<Client> {
  public:
    Client(int index, boost::asio::io_context &io_context)
        : userIdx_(index), socket_(io_context), resolver_(io_context) {}

    void start() {
        file_.open(("./test_case/" + connections[userIdx_].file), ios::in); // Open file
        doResolve();
    }

  private:
    void doResolve() {
        auto self(shared_from_this());
        resolver_.async_resolve(
            connections[userIdx_].host,
            connections[userIdx_].port,
            [this, self](boost::system::error_code ec, tcp::resolver::iterator it) {
                if (!ec) {
                    doConnect(it);
                }
            });
    }

    void doConnect(tcp::resolver::iterator it) {
        auto self(shared_from_this());
        socket_.async_connect(
            *it,
            [this, self](boost::system::error_code ec) {
                if (!ec) {
                    doRead();
                }
            });
    }

    void doRead() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    string content(data_, length);
                    // Clear read data
                    memset(data_, '\0', max_length);

                    outputShell(content);

                    if (content.find("% ") != string::npos) {
                        doWrite();
                    }
                    else {
                        doRead();
                    }
                }
            });
    }

    void doWrite() {
        auto self(shared_from_this());
        string command = getCommand();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(command.c_str(), command.length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    doRead();
                }
            });
    }

    string getCommand() {
        string command;
        if (file_.is_open()) {
            getline(file_, command);
            if (command.find("exit") != string::npos) {
                file_.close();
            }
            command += "\n";
            outputCommand(command);
        }
        return command;
    }

    string htmlEscape(string content) {
        boost::replace_all(content, "&", "&amp;");
        boost::replace_all(content, "\"", "&quot;");
        boost::replace_all(content, "\'", "&apos;");
        boost::replace_all(content, "<", "&lt;");
        boost::replace_all(content, ">", "&gt;");
        boost::replace_all(content, "\n", "&NewLine;");
        boost::replace_all(content, "\r", "");
        boost::replace_all(content, " ", "&nbsp;");
        return content;
    }

    void outputShell(string content) {
        string contentEsc = htmlEscape(content);
        cout << "<script>document.getElementById('s" << userIdx_ << "').innerHTML += '" << contentEsc << "';</script>" << flush;
    }

    void outputCommand(string content) {
        string contentEsc = htmlEscape(content);
        cout << "<script>document.getElementById('s" << userIdx_ << "').innerHTML += '<b>" << contentEsc << "</b>';</script>" << flush;
    }

    int userIdx_;
    tcp::socket socket_;
    tcp::resolver resolver_;
    fstream file_;
    enum { max_length = 1024 };
    char data_[max_length];
};

void parseQueryString() {
    vector<string> tmp;
    string queryString = getenv("QUERY_STRING");
    boost::split(tmp, queryString, boost::is_any_of("&"));
    for (unsigned long int i = 0; i < tmp.size(); i++) {
        vector<string> tmp2;
        boost::split(tmp2, tmp[i], boost::is_any_of("="));
        if (tmp2.size() == 2) {
            if (tmp2[0][0] == 'h') {
                connections[i / 3].host = tmp2[1];
            }
            else if (tmp2[0][0] == 'p') {
                connections[i / 3].port = tmp2[1];
            }
            else if (tmp2[0][0] == 'f') {
                connections[i / 3].file = tmp2[1];
            }
        }
    }
}

void createConsole() {
    cout << contentType;
    cout << contentHead;
    cout << contentBodyFront;
    for (int i = 0; i < MAX_CONNECTION; i++) {
        if (connections[i].host == "") {
            break;
        }
        cout << "<th scope=\"col\">" << connections[i].host << ":" << connections[i].port << "</th>";
    }
    cout << contentBodyMiddle;
    for (int i = 0; i < MAX_CONNECTION; i++) {
        if (connections[i].host == "") {
            break;
        }
        cout << "<td><pre id=\"s" << i << "\" class=\"mb-0\"></pre></td>";
    }
    cout << contentBodyEnd;
}

void makeConnection(boost::asio::io_context &io_context) {
    for (int idx = 0; idx < MAX_CONNECTION; idx++) {
        if (connections[idx].host == "") {
            return;
        }

        std::make_shared<Client>(idx, io_context)->start();
    }
}

int main(int argc, char *argv[]) {
    try {
        boost::asio::io_context io_context;

        parseQueryString();
        createConsole();
        makeConnection(io_context);

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}