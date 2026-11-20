#include <iostream>
#include <vector>
#include <cstdint>

/*
    goal: measure how long CPU needs to wait when it reads 
    one piece of data from memory.
*/

// list of sizes for buffer
std::vector<std::uint32_t> buffer_size_list{4096, 1048576, 8388608, 268435456};

// struct representing an element
// size = 64 bytes = 1 cache line
template <typename T>
struct alignas(64) cache_line_node{
    // inner data
    struct payload{
        T value{};
        std::uint64_t next_element_addr{};
    };
    payload<T> cur_payload{};
    // check if the current payload is <= 64 bytes
    static_assert(sizeof(cur_payload) <= 64, "The element's data size is bigger than 64 bytes!");
};