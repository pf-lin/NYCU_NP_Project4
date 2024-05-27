#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

using boost::asio::ip::tcp;
using namespace std;

#define SOCKS_VERSION 4
#define SOCKS_CONNECT 1
#define SOCKS_BIND 2
#define SOCKS_GRANTED 90
#define SOCKS_REJECTED 91
#define REQUEST_PACKET_SIZE 264
#define REPLY_PACKET_SIZE 8

struct SocketsPacket {
    int VN;
    int CD;
    string DSTPORT;
    string DSTIP;
    string DOMAIN_NAME;
};

class Session : public std::enable_shared_from_this<Session> {
  public:
    Session(tcp::socket socket, boost::asio::io_context &io_context)
        : clientSocket_(std::move(socket)), serverSocket_(io_context), resolver_(io_context) {}

    void start() {
        doRead();
    }

  private:
    // First time read (SOCKS request)
    // Client (cgi) --> SOCKS Server
    void doRead() {
        auto self(shared_from_this());
        clientSocket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    parseSocksRequest();

                    if (socksPacket.CD == SOCKS_CONNECT) {
                        socksConnect();
                    }
                    else if (socksPacket.CD == SOCKS_BIND) {
                        socksBind();
                    }
                }
            });
    }

    void socksConnect() {
        doResolve(getHost());
    }

    void socksBind() {
        auto self(shared_from_this());
        // TODO: Implement SOCKS BIND
    }

    void doResolve(string host) {
        auto self(shared_from_this());
        resolver_.async_resolve(
            host,
            socksPacket.DSTPORT,
            [this, self, host](boost::system::error_code ec, tcp::resolver::iterator it) {
                if (!ec) {
                    doConnect(it);
                }
                else {
                    doReject();
                }
            });
    }

    // Client (cgi) --- SOCKS Server <===> Server (RAS/RWG)
    void doConnect(tcp::resolver::iterator it) {
        auto self(shared_from_this());
        serverSocket_.async_connect(
            *it,
            [this, self](boost::system::error_code ec) {
                if (!ec) {
                    sendSocksReply(SOCKS_GRANTED);
                }
                else {
                    doReject();
                }
            });
    }

    void doReject() {
        sendSocksReply(SOCKS_REJECTED);
    }

    // Client (cgi) <-- SOCKS Server
    void sendSocksReply(int reply) {
        auto self(shared_from_this());
        memset(reply_, 0, REPLY_PACKET_SIZE);
        reply_[0] = 0;
        reply_[1] = reply;                                                              // SOCKS_GRANTED or SOCKS_REJECTED
        reply_[2] = socksPacket.CD == SOCKS_BIND ? stoi(socksPacket.DSTPORT) / 256 : 0; // DSTPORT
        reply_[3] = socksPacket.CD == SOCKS_BIND ? stoi(socksPacket.DSTPORT) % 256 : 0; // DSTPORT
        reply_[4] = 0;                                                                  // DSTIP
        reply_[5] = 0;                                                                  // DSTIP
        reply_[6] = 0;                                                                  // DSTIP
        reply_[7] = 0;                                                                  // DSTIP
        boost::asio::async_write(
            clientSocket_,
            boost::asio::buffer(reply_, REPLY_PACKET_SIZE),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    if (reply_[1] == SOCKS_GRANTED) {
                        printSocksServerMessages();
                        doReadClient();
                        doReadServer();
                    }
                    else {
                        clientSocket_.close();
                    }
                }
            });
    }

    // Client (cgi) --> SOCKS Server --- Server (RAS/RWG)
    void doReadClient() {
        auto self(shared_from_this());
        memset(clientData_, '\0', max_length);
        clientSocket_.async_read_some(
            boost::asio::buffer(clientData_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    doWriteServer(length);
                }
            });
    }

    // Client (cgi) --- SOCKS Server <-- Server (RAS/RWG)
    void doReadServer() {
        auto self(shared_from_this());
        memset(serverData_, '\0', max_length);
        serverSocket_.async_read_some(
            boost::asio::buffer(serverData_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    doWriteClient(length);
                }
            });
    }

    // Client (cgi) <-- SOCKS Server --- Server (RAS/RWG)
    void doWriteClient(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            clientSocket_,
            boost::asio::buffer(serverData_, length),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    doReadServer();
                }
            });
    }

    // Client (cgi) --- SOCKS Server --> Server (RAS/RWG)
    void doWriteServer(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            serverSocket_,
            boost::asio::buffer(clientData_, length),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    doReadClient();
                }
            });
    }

    void parseSocksRequest() {
        socksPacket.VN = data_[0];
        socksPacket.CD = data_[1];
        socksPacket.DSTPORT = to_string((data_[2] << 8) + data_[3]); // data_[2] * 256 + data_[3]
        socksPacket.DSTIP = to_string(data_[4]) + "." + to_string(data_[5]) + "." + to_string(data_[6]) + "." + to_string(data_[7]);
        int i = 9;
        while (data_[i] != 0) {
            socksPacket.DOMAIN_NAME += data_[i];
            i++;
        }
    }

    string getHost() {
        vector<string> dstIp;
        boost::split(dstIp, socksPacket.DSTIP, boost::is_any_of("."));
        if (dstIp[0] == "0" && dstIp[1] == "0" && dstIp[2] == "0" && dstIp[3] != "0") {
            return socksPacket.DOMAIN_NAME;
        }
        return socksPacket.DSTIP;
    }

    void printSocksServerMessages() {
        cout << "<S_IP>: " << clientSocket_.remote_endpoint().address() << endl;
        cout << "<S_PORT>: " << clientSocket_.remote_endpoint().port() << endl;
        cout << "<D_IP>: " << socksPacket.DSTIP << endl;
        cout << "<D_PORT>: " << socksPacket.DSTPORT << endl;
        cout << "<Command>: " << (socksPacket.CD == SOCKS_CONNECT ? "CONNECT" : "BIND") << endl;
        cout << "<Reply>: " << (reply_[1] == SOCKS_GRANTED ? "Accept" : "Reject") << endl;
    }

    tcp::socket clientSocket_;
    tcp::socket serverSocket_;
    tcp::resolver resolver_;
    enum { max_length = 1024 };
    unsigned char data_[max_length];
    unsigned char clientData_[max_length];
    unsigned char serverData_[max_length];
    unsigned char reply_[REPLY_PACKET_SIZE];
    SocketsPacket socksPacket;
};

class Server {
  public:
    Server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), io_context_(io_context) {
        doAccept();
    }

  private:
    void doAccept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    io_context_.notify_fork(boost::asio::io_context::fork_prepare);
                    pid_t pid = fork();
                    if (pid == 0) {
                        io_context_.notify_fork(boost::asio::io_context::fork_child);
                        std::make_shared<Session>(std::move(socket), io_context_)->start();
                    }
                    else {
                        io_context_.notify_fork(boost::asio::io_context::fork_parent);
                        socket.close();
                        doAccept();
                    }
                }
            });
    }

    tcp::acceptor acceptor_;
    boost::asio::io_context &io_context_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: async_socks_server <port>\n";
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