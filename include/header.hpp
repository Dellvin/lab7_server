// Copyright 2018 Your Name <your_email>

#ifndef INCLUDE_HEADER_HPP_
#define INCLUDE_HEADER_HPP_

#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <chrono>
#include <thread>
#include <signal.h>
#include <csignal>
#include <sys/wait.h>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/expressions.hpp>


#define BOOST_ASIO_SEPARATE_COMPILATION
namespace logging = boost::log;
namespace keywords = boost::log::keywords;

using boost::posix_time::ptime;
using std::cout;
using std::endl;

typedef boost::shared_ptr <boost::asio::ip::tcp::socket> socket_ptr;
ptime lastActiveTime;

class clientInterection;

std::vector <clientInterection> users;
std::mutex muter;
enum messageType {
    MESSAGE_NOT_RECOGNIZED,
    MESSAGE_PING,
    MESSAGE_CLIENT
};


class clientInterection {
public:
    explicit clientInterection(socket_ptr sock) : socket(sock) {
        init_logging();
        BOOST_LOG_TRIVIAL(info) << "the client has been connected" << endl;
        setLogin();
        BOOST_LOG_TRIVIAL(info) << "Login: " << login << endl;
        messageHandler();
        BOOST_LOG_TRIVIAL(info)
        << "Time out is over\nClient has been disconnected" << endl;
        socket->write_some(boost::asio::buffer("bye\n"));
        socket->close();
    }

    void setLogin() {
        bool loginFlag;
        do {
            char log[32];
            memset(log, '\0', 32);
            socket->read_some(boost::asio::buffer(log));
            if (!checkMessage(log)) {
                socket->write_some(boost::asio::buffer("login_not_ok\n"));
                loginFlag = false;
            } else {
                login = log;
                socket->write_some(boost::asio::buffer("login_ok\n"));
                loginFlag = true;
            }
        } while (!loginFlag);
    }

    void messageHandler() {
        if (!fork()) {
            while (true) {
                char data[1024];
                memset(data, '\0', 1024);
                try {
                    alarm(5);
                    socket->read_some(boost::asio::buffer(data));
                    BOOST_LOG_TRIVIAL(info) << login << ": " << data << endl;
                } catch (...) {
                    BOOST_LOG_TRIVIAL(info) << login
                    << ": " << "Client was disconected" << endl;
                    exit(0);
                }
                if (!checkMessage(data)) {
                    BOOST_LOG_TRIVIAL(info)
                    << login << ": " << "error message" << endl;
                    socket->write_some(boost::asio::buffer("error message\n"));
                    continue;
                }
                switch (messageIdentification(data)) {
                    case MESSAGE_PING:
                        answerOnPing();
                        break;
                    case MESSAGE_CLIENT:
                        sendClientList();
                        break;
                    case MESSAGE_NOT_RECOGNIZED:
                        undefindCommand();
                        break;
                }
            }
        }
        wait(nullptr);
    }

    void undefindCommand() {
        BOOST_LOG_TRIVIAL(info)
        << login << ": " << "Undefind command" << endl;
        socket->write_some(boost::asio::buffer("Undefind command\n"));
    }

    void answerOnPing() {
        BOOST_LOG_TRIVIAL(info) << login << ": " << "ping_ok" << endl;
        socket->write_some(boost::asio::buffer("ping_ok\n"));
    }

    void sendClientList() {
        std::string clientsList;
        for (uint64_t i = 0; i < users.size(); i++) {
            clientsList.append(users.at(i).login);
        }
        clientsList.append(login);
        BOOST_LOG_TRIVIAL(info)
        << "clients: " << std::endl << clientsList << endl;
        socket->write_some(boost::asio::buffer(clientsList));
    }

    bool checkMessage(char *mes) {
        std::string m = mes;
        if (m.find('\n') != 0) {
            uint64_t pos = m.find("\n");
            for (uint64_t i = pos; i < m.size(); i++) {
                m.at(i) = '\0';
            }
            return true;
        }
        return false;
    }

    messageType messageIdentification(char *mes) {
        std::string m = mes;
        if (m == "ping\n") return MESSAGE_PING;
        if (m == "clients\n") return MESSAGE_CLIENT;
        return MESSAGE_NOT_RECOGNIZED;
    }

    static void init_logging() {
        logging::register_simple_formatter_factory<logging
        ::trivial::severity_level, char>("Severity");

        logging::add_file_log
                (
                        keywords::file_name = "info.log",
                        keywords::format =
                        "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%");
        logging::add_console_log(
                std::cout,
                keywords::format =
                "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%");
        logging::add_common_attributes();
    }

    std::string login;
    socket_ptr socket;
    sig_atomic_t timeout = 0;
    ptime lastPingTime;
};

void funcForClientInterection(socket_ptr sock) {
    muter.lock();
    clientInterection newClient(sock);
    users.push_back(newClient);
    muter.unlock();
}

void acceptor() {
    boost::asio::io_service service;
    boost::asio::ip::tcp::endpoint
    ep(boost::asio::ip::address::from_string("127.0.0.1"), 1024);
    // before this number port does not works
    boost::asio::ip::tcp::acceptor acc(service, ep);
    std::cout << "I am ready" << std::endl;
    while (true) {
        socket_ptr sock(new boost::asio::ip::tcp::socket(service));
        acc.accept(*sock);
        std::thread workWithClient(funcForClientInterection, sock);
        workWithClient.join();
    }
}
#endif // INCLUDE_HEADER_HPP_
