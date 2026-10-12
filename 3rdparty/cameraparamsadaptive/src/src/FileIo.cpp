/**
 * @file FileIo.cpp
 * @brief Implementation of the text file I/O helpers.
 */

#include "camadapt/FileIo.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace camadapt {

std::string readTextFile(const std::string &path) {
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if(!in.is_open()) {
        throw std::runtime_error("cannot open file for reading: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if(in.bad()) {
        throw std::runtime_error("failed while reading file: " + path);
    }
    return content;
}

std::string padToEvenByteSize(std::string content) {
    if(content.size() % 2 != 0) {
        content.push_back(kStoragePadByte);
    }
    return content;
}

bool hasEvenByteSize(const std::string &path) {
    try {
        return readTextFile(path).size() % 2 == 0;
    } catch(const std::exception &) {
        return false;
    }
}

bool writeTextFile(const std::string &path, const std::string &content) {
    // "wb": binary mode, so a raw '\n' reaches the file as exactly one byte on
    // every platform (no CRLF translation by the C runtime on Windows).
    const std::string padded = padToEvenByteSize(content);
    std::ofstream out(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if(!out.is_open()) {
        return false;
    }
    out.write(padded.data(), static_cast<std::streamsize>(padded.size()));
    out.flush();
    return static_cast<bool>(out);
}

}  // namespace camadapt
