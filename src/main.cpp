#include "tui/app.h"

#include <clocale>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // Enable UTF-8 support from the system locale.
    std::setlocale(LC_ALL, "");

    try {
        if (argc > 1) {
            berezta::App app(argv[1]);
            return app.run();
        } else {
            berezta::App app;
            return app.run();
        }
    } catch (const std::exception& e) {
        std::cerr << "berezta: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
