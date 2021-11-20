#pragma once

/**
 * Wrapper template for a deleter function that accept a pointer to the struct to free
 * "auto" will accept any signature for the "deleter" function (e.g. void (*deleter)(Struct *)),
 * creating a different struct for each one of them at compile-time
 */
template <auto deleter>
struct DeleterP {
    template <typename T>
    void operator()(T* t) const {
        if (t) deleter(t);
    }
};

/*
 * Wrapper template for a deleter function that accept a pointer-to-pointer to the struct to free
 * "auto" will accept any signature for the "deleter" function (e.g. void (*deleter)(StructPtr **)),
 * creating a different struct for each one of them at compile-time
 */
template <auto deleter>
struct DeleterPP {
    template <typename T>
    void operator()(T* t) const {
        if (t) deleter(&t);
    }
};

/**
 * To use unique pointers with these custom deleters wrappers follow this approach:
 * std::unique_ptr<Struct, Deleter<deleter_func>> ptr(obj_ptr)
 */