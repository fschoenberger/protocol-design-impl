#pragma once

#include <boost/asio.hpp>
#include <random>

#include "pch.hpp"
#include "messages.hpp"

namespace rft {

class Stream {
    enum class State {
        kWaitingForServerHello,
        kStreamEstablished,
        kFinSent,
        kFinReceived,
        kEnd
    };

public:
    explicit Stream(boost::asio::ip::udp::socket* socket);

    void ProcessMessage(const MessageBase* m);

private:
    void SendServerHello();

    boost::asio::ip::udp::socket* socket_;
};

class Client {
public:
    Client(const boost::asio::any_io_executor& executionContext);

    boost::asio::awaitable<void> Send();
private:
    constexpr static auto MAX_LENGTH = 1024 - 8;

    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint senderEndpoint_;
    std::array<char, MAX_LENGTH> data_;

    ClientHello hello_{0, MessageType::kClientHello, 0, 0x1, 0, 0, 10, 0, "C:\\test.txt"};
};

}
