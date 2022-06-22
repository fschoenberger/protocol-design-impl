#include "pch.hpp"

#include <iostream>
#include <boost/program_options.hpp>

namespace options = boost::program_options;

int main(int argc, char** argv) {
	options::options_description desc("Allowed options");
	desc.add_options()
		("help", "Print help message")
		("version", "Print version");

	options::variables_map map;
	options::store(options::parse_command_line(argc, argv, desc), map);
	options::notify(map);

	if(map.contains("help")) {
	    std::cout << desc << "\n";
		return 1;
	}

	std::cout << "Hello from client!";
}
