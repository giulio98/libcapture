#pragma once

#include <exception>

struct BufferFullException : public std::exception {
    const char* what() const throw() { return "Internal buffer is full, read from it and re-try to send"; }
};

struct BufferEmptyException : public std::exception {
    const char* what() const throw() { return "Internal buffer is empty, send data and re-try to read"; }
};

struct BufferFlushedException : public std::exception {
    const char* what() const throw() { return "Internal buffer has been flushed"; }
};