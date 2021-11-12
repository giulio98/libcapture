#pragma once

#include <exception>

struct FullException : public std::exception {
    const char* what() const throw() { return "Internal buffer is full, read and re-try to send"; }
};

struct EmptyException : public std::exception {
    const char* what() const throw() { return "Internal buffer is empty, send and re-try to read"; }
};