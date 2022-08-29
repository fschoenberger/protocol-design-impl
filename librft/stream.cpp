#include "pch.hpp"

#include "stream.hpp"
#include "logger.hpp"

#include <iostream>

//static constexpr auto SERVER_PORT = 10251;

using namespace rft;
namespace ip = boost::asio::ip;

Stream::Stream(boost::asio::ip::udp::socket* socket)
    : socket_(socket) {

}

void Stream::ProcessMessage(const MessageBase* m) {
    switch (m->messageType) {
        case MessageType::kClientHello:
            SendServerHello();
            //We need to generate a server hello
            break;
        case MessageType::kServerHello:
            //SendAck();
            break;
        case MessageType::kAck:
            break;
        case MessageType::kFin:
            break;
        case MessageType::kError:
            break;
        case MessageType::kChunk:
            break;
        default: ;
    }
}

void Stream::SendServerHello() {
    /*const auto* serverHello = new ServerHello{0x1, MessageType::kServerHello, 0, 0x2, 0, 0, 10, {0}, 0, 200};
    socket_->async_send_to(nullptr, endpoint, [&](auto ec, auto s) {
        LOG_DEBUG("Sent {} bytes to {}", s, endpoint.address().to_string());
        delete serverHello;
    });*/
}


Client::Client(const boost::asio::any_io_executor& executionContext)
    : socket_(executionContext, ip::udp::endpoint(ip::udp::v4(), 0)) {
    boost::asio::co_spawn(executionContext, Send(), boost::asio::detached);
}

boost::asio::awaitable<void> Client::Send() {
    co_await socket_.async_send_to(boost::asio::buffer(&hello_, sizeof(hello_)), ip::udp::endpoint(ip::address::from_string("127.0.0.2"), 5051), boost::asio::use_awaitable);
    
    for(;;) {
        char buf[1024];
        auto nBytes = co_await socket_.async_receive(boost::asio::buffer(buf, 1024), boost::asio::use_awaitable);
        LOG_INFO("Received {} bytes from Server.", nBytes);
    }

    co_return;
}

//void Server::Send() {
//    socket_.send_to(boost::asio::buffer(data_, MAX_LENGTH), senderEndpoint_);
//}
