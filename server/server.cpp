#include "../librft/server.hpp"

#include <boost/program_options.hpp>
#include <iostream>
#include <filesystem>
#include "pch.hpp"

namespace options = boost::program_options;

std::vector<std::string> files; // currently available files

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


    std::string path = getenv("USERPROFILE");
    path += "\\RFT";
    //auto dirIter = std::filesystem::directory_iterator(path);
    int fileCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(path)) fileCount++;

    if (fileCount == 0) std::cout << "Please put files into the " << path << " folder!\n";

    std::cout << "Currently serving the following " << fileCount << " files:\n";
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        std::cout << entry.path() << std::endl;
        files.push_back(entry.path().string());
    }

    ioContext.join();

    std::cout << "Goodbye from server.\n";
}
