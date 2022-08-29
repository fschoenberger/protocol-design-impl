#include "pch.hpp"
#include "client.hpp"

namespace ip = boost::asio::ip;

namespace rft {

Client::Client(boost::asio::any_io_executor executor)
    : socket_(executor, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
      executor_(executor) {

}

boost::asio::awaitable<void> Client::Run() {
    ClientHello hello_{0, MessageType::kClientHello, 0, 0x1, 0, 0, 10, 0, {"C:\\source\\blog\\src\\images\\technology.jpg"}};

    co_await socket_.async_send_to(boost::asio::buffer(&hello_, sizeof(hello_)), ip::udp::endpoint(ip::address::from_string("127.0.0.2"), 5051), boost::asio::use_awaitable);

    for (;;) {
        char buf[1024];
        auto nBytes = co_await socket_.async_receive(boost::asio::buffer(buf, 1024), boost::asio::use_awaitable);
        LOG_INFO("Received {} bytes from Server.", nBytes);
    }

    co_return;
}

}
