#include <exception>
#include <iostream>
#include <string>

void sift_test1B(int subset_size_millions);

int main(int argc, char **argv) {
    int subset_size_millions = 50;
    const std::string prefix = "--subset-size=";

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.rfind(prefix, 0) == 0) {
            const std::string value_str = arg.substr(prefix.size());
            int value = 0;
            try {
                value = std::stoi(value_str);
            } catch (const std::exception &) {
                std::cerr << "Invalid subset size '" << value_str << "'" << std::endl;
                std::cerr << "Allowed values: 20, 50, 100" << std::endl;
                return 1;
            }

            if (value == 20 || value == 50 || value == 100) {
                subset_size_millions = value;
            } else {
                std::cerr << "Invalid subset size '" << value << "'. Allowed values: 20, 50" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            std::cerr << "Usage: " << argv[0] << " [--subset-size=20|50|100]" << std::endl;
            return 1;
        }
    }

    sift_test1B(subset_size_millions);
    return 0;
}
