#include "../librft/server.hpp"

#include <boost/program_options.hpp>
#include <iostream>

#include "../librft/stream.hpp"
#include "pch.hpp"

namespace options = boost::program_options;

int main(int argc, char** argv) {
    options::options_description desc("Allowed options");
    desc.add_options()("help", "Print help message")("version", "Print version");

    options::variables_map map;
    options::store(options::parse_command_line(argc, argv, desc), map);
    options::notify(map);

    if (map.count("help") > 0) {
        std::cout << desc << "\n";
        return 1;
    }

    boost::asio::thread_pool ioContext;
    rft::Server s(ioContext.get_executor(), 5051);
    boost::asio::co_spawn(ioContext, s.Run(), boost::asio::detached);

    std::cout << "________________________________\n"
              << "\\______   \\_   _____/\\__    ___/\n"
              << "|       _/|    __)    |    |\n"
              << "|    |   \\|     \\     |    |\n"
              << "|____|_  /\\___  /     |____|\n"
              << "       \\/     \\/          \n"
              << "\nRFT server reference implementation. \n(c) 2022 Alexander Maslew, Frederic Schoenberger\n\n";

    ioContext.join();

    std::cout << "Goodbye from server.\n";
}
