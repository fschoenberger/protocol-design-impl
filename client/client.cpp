#include "pch.hpp"

#include <iostream>
#include <boost/program_options.hpp>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <thread>

#include "../librft/stream.hpp"
#include "../librft/logger.hpp"

namespace options = boost::program_options;

using boost::asio::ip::udp;
using boost::asio::ip::address;

#define IPADDRESS "127.0.0.1"
#define UDP_PORT 10251



struct Client {
	boost::asio::io_service io_service;
	udp::socket socket{io_service};
    boost::array<char, 1024> recv_buffer;
    udp::endpoint remote_endpoint;

    int count = 3;

    void handle_receive(const boost::system::error_code& error, size_t bytes_transferred) {
        if (error) {
            std::cout << "Receive failed: " << error.message() << "\n";
            return;
        }
        std::cout << "Received: '" << std::string(recv_buffer.begin(), recv_buffer.begin()+bytes_transferred) << "' (" << error.message() << ")\n";

        if (--count > 0) {
            std::cout << "Count: " << count << "\n";
            wait();
        }
    }

    void wait() {
        socket.async_receive_from(boost::asio::buffer(recv_buffer),
            remote_endpoint,
            boost::bind(&Client::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }

    void Receiver()
    {
        socket.open(udp::v4());
        socket.bind(udp::endpoint(address::from_string(IPADDRESS), UDP_PORT));

        wait();

        std::cout << "Receiving\n";
        io_service.run();
        std::cout << "Receiver exit\n";
    }
};



int main(int argc, char** argv) {
	options::options_description desc("Allowed options");
	desc.add_options()
		("help", "Print help message")
		("version", "Print version");

	options::variables_map map;
	options::store(options::parse_command_line(argc, argv, desc), map);
	options::notify(map);

	if(map.count("help") > 0) {
	    std::cout << desc << "\n";
		return 1;
	}

    LOG_INFO("Starting client!");

    boost::asio::io_context ioContext;
    rft::Client s(ioContext.get_executor());
    ioContext.run();

	LOG_INFO("Hello from client!");
}
