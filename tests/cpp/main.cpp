#include <exception>
#include <iostream>
#include <string>

void sift_test1B(int subset_size_millions, int max_pool_percent, const std::string& output_path);

int main(int argc, char **argv) {
    int subset_size_millions = 50;
    int max_pool_percent = 20;  // Default value
    std::string output_path = ".";  // Default to current directory
    const std::string subset_prefix = "--subset-size=";
    const std::string maxpool_prefix = "--max-pool-percent=";
    const std::string output_prefix = "--output-path=";

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.rfind(subset_prefix, 0) == 0) {
            const std::string value_str = arg.substr(subset_prefix.size());
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
                std::cerr << "Invalid subset size '" << value << "'. Allowed values: 20, 50, 100" << std::endl;
                return 1;
            }
        } else if (arg.rfind(maxpool_prefix, 0) == 0) {
            const std::string value_str = arg.substr(maxpool_prefix.size());
            int value = 0;
            try {
                value = std::stoi(value_str);
            } catch (const std::exception &) {
                std::cerr << "Invalid max pool percent '" << value_str << "'" << std::endl;
                std::cerr << "Allowed values: 10, 20, 30, 40, 50, 60" << std::endl;
                return 1;
            }

            if (value == 10 || value == 20 || value == 30 || value == 40 || value == 50 || value == 60) {
                max_pool_percent = value;
            } else {
                std::cerr << "Invalid max pool percent '" << value << "'. Allowed values: 10, 20, 30, 40, 50, 60" << std::endl;
                return 1;
            }
        } else if (arg.rfind(output_prefix, 0) == 0) {
            output_path = arg.substr(output_prefix.size());
            // Basic validation - check if path is not empty
            if (output_path.empty()) {
                std::cerr << "Output path cannot be empty" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            std::cerr << "Usage: " << argv[0] << " [--subset-size=20|50|100] [--max-pool-percent=10|20|30|40|50|60] [--output-path=path]" << std::endl;
            return 1;
        }
    }

    sift_test1B(subset_size_millions, max_pool_percent, output_path);
    return 0;
}
