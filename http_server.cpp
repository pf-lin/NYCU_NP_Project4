#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

using boost::asio::ip::tcp;
using namespace std;

struct Environment {
    string REQUEST_METHOD = "";
    string REQUEST_URI = "";
    string PATH_INFO = "";
    string QUERY_STRING = "";
    string SERVER_PROTOCOL = "";
    string HTTP_HOST = "";
    string SERVER_ADDR = "";
    string SERVER_PORT = "";
    string REMOTE_ADDR = "";
    string REMOTE_PORT = "";
};

const string HTTP_OK = "HTTP/1.1 200 OK\r\n";

class Session : public std::enable_shared_from_this<Session> {
  public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        do_read();
    }

  private:
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    parseHTTPRequest();
                    createResponse();
                }
            });
    }

    void parseHTTPRequest() {
        stringstream ss(data_);
        ss >> envVars.REQUEST_METHOD >> envVars.REQUEST_URI >> envVars.SERVER_PROTOCOL;

        string temp;
        ss >> temp;
        if (temp == "Host:") {
            ss >> envVars.HTTP_HOST;
        }

        // Extract query string
        size_t pos = envVars.REQUEST_URI.find("?");
        if (pos != string::npos) {
            envVars.QUERY_STRING = envVars.REQUEST_URI.substr(pos + 1);
            envVars.PATH_INFO = envVars.REQUEST_URI.substr(0, pos);
        }
        else {
            envVars.PATH_INFO = envVars.REQUEST_URI;
        }

        envVars.SERVER_ADDR = socket_.local_endpoint().address().to_string();
        envVars.SERVER_PORT = to_string(socket_.local_endpoint().port());
        envVars.REMOTE_ADDR = socket_.remote_endpoint().address().to_string();
        envVars.REMOTE_PORT = to_string(socket_.remote_endpoint().port());
    }

    void setEnv() {
        setenv("REQUEST_METHOD", envVars.REQUEST_METHOD.c_str(), 1);
        setenv("REQUEST_URI", envVars.REQUEST_URI.c_str(), 1);
        setenv("QUERY_STRING", envVars.QUERY_STRING.c_str(), 1);
        setenv("SERVER_PROTOCOL", envVars.SERVER_PROTOCOL.c_str(), 1);
        setenv("HTTP_HOST", envVars.HTTP_HOST.c_str(), 1);
        setenv("SERVER_ADDR", envVars.SERVER_ADDR.c_str(), 1);
        setenv("SERVER_PORT", envVars.SERVER_PORT.c_str(), 1);
        setenv("REMOTE_ADDR", envVars.REMOTE_ADDR.c_str(), 1);
        setenv("REMOTE_PORT", envVars.REMOTE_PORT.c_str(), 1);
    }

    void createResponse() {
        pid_t pid = fork();
        if (pid < 0) {
            cout << "Error forking" << endl;
        }
        else if (pid == 0) {
            setEnv();
            dup2(socket_.native_handle(), STDIN_FILENO);
            dup2(socket_.native_handle(), STDOUT_FILENO);
            socket_.close();

            cout << HTTP_OK << flush;

            string path = "." + envVars.PATH_INFO;
            cout << envVars.PATH_INFO << endl;
            cout << path << endl;
            if (execlp(path.c_str(), path.c_str(), NULL) == -1) {
                cout << "Error executing script" << endl;
            }
        }
        else {
            socket_.close();
        }
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
    Environment envVars;
};

class Server {
  public:
    Server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

  private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: async_tcp_server <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        Server s(io_context, std::atoi(argv[1]));

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}