#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

class Config {
public:
    static void initializeConfig(const std::string& filePath);
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    void load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open the file!" << std::endl;
            return;
        }

        std::string line, section;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line[0] == '[') {
                section = line.substr(1, line.find(']') - 1);
            } else {
                std::istringstream is_line(line);
                std::string key;
                if (std::getline(is_line, key, '=')) {
                    std::string value;
                    if (std::getline(is_line, value)) {
                        configData[section + '.' + key] = value;
                    }
                }
            }
        }
    }

    std::string getValue(const std::string& section, const std::string& key) {
       std::string fullKey = section + '.' + key;
        if (configData.find(fullKey) != configData.end()) {
            return configData[fullKey];
        } else {
            std::cerr << "Key not found: " << fullKey << std::endl;
            return ""; 
        }
    }

    void trim(std::string &str) {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), str.end());
    }

    void parseMac(const std::string &macStr, uint8_t *mac) {
    	std::istringstream ss(macStr);
    	std::string byteStr;
    	int i = 0;

    	while (std::getline(ss, byteStr, ':') && i < 6) {
            std::istringstream hexByte(byteStr);
            int byte;
            hexByte >> std::hex >> byte;
            mac[i++] = static_cast<uint8_t>(byte);
    	}	
    }

    // Prevent copy and move operations
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

private:
    Config() = default;
    std::map<std::string, std::string> configData;
};

#endif // CONFIG_H


