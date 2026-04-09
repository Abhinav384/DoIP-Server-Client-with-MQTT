// Config.cpp
#include "config.h"

// Define a function to initialize the config
void initializeConfig(const std::string& filename) {
    Config& config = Config::getInstance();
    config.load(filename);
}

