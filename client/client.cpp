#include "pch.hpp"

#include <iostream>
#include <string>
#include <boost/program_options.hpp>

#include <boost/asio.hpp>

#include "../librft/client.hpp"
#include "../librft/logger.hpp"

namespace options = boost::program_options;

using boost::asio::ip::udp;
using boost::asio::ip::address;

int main(int argc, char** argv) {
    options::options_description desc("Allowed options");
    desc.add_options()
        ("help", "Print help message")
        ("version", "Print version");

    options::variables_map map;
    options::store(options::parse_command_line(argc, argv, desc), map);
    options::notify(map);

    if (map.count("help") > 0) {
        std::cout << desc << "\n";
        return 1;
    }

    boost::asio::thread_pool ioContext;
    rft::Client s(ioContext.get_executor());

    std::cout << "________________________________\n"
        << "\\______   \\_   _____/\\__    ___/\n"
        << "|       _/|    __)    |    |\n"
        << "|    |   \\|     \\     |    |\n"
        << "|____|_  /\\___  /     |____|\n"
        << "       \\/     \\/          \n"
        << "\nRFT client reference implementation. \n(c) 2022 Alexander Maslew, Frederic Schoenberger\n\n";


    std::cout << "Downloaded files will be saved to your Desktop\n"
        << "Please enter fileName to download:";
    std::string fileName;
    std::getline(std::cin, fileName);
    LOG_INFO("Starting client!");

    boost::asio::co_spawn(ioContext, s.Run(fileName), boost::asio::detached);
    ioContext.join();

    LOG_INFO("Goodbye from client.");
}
