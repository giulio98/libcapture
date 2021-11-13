#pragma once

#include <exception>

class AVException : public std::runtime_error {
public:
    AVException(const std::string &msg) : std::runtime_error(msg) {}
};

class FullException : public AVException {
public:
    FullException(const std::string &item_name)
        : AVException(item_name + " is full, read from it and re-try to send") {}
};

class EmptyException : public AVException {
public:
    EmptyException(const std::string &item_name)
        : AVException(item_name + " is empty, send something to it and re-try to read") {}
};

class FlushedException : public AVException {
public:
    FlushedException(const std::string &item_name) : AVException(item_name + " has been flushed") {}
};