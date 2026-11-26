#ifndef CACHE_PROBE_H
#define CACHE_PROBE_H

#include <iostream>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <random>
#include <format>
#include <chrono>
#include <x86intrin.h>
#include <thread>

namespace fs = std::filesystem;

constexpr std::size_t g_alignment = 64;
constexpr std::size_t g_cache_line_size = 64;

/*
    caller will call cache_probe every 100ms.
    caller will decide the buffer size.
*/
class cache_probe{
    private:
        struct alignas(g_alignment) cache_line_node{
            cache_line_node* next{};
        };
        std::size_t m_buffer_size{};
        std::size_t m_num_nodes{};
        std::vector<cache_line_node> m_buffer{};
        std::size_t m_loads_per_call{};
        double m_num_tsc_ticks_per_ns{};
        // store the last position in the buffer
        cache_line_node* m_cur_node{};
    public:
        // default constuctor
        cache_probe() = delete;
        // parameterized constructor
        cache_probe(std::size_t buffer_size);
        // copy constructor
        cache_probe(const cache_probe& neigh_cache_probe) = delete;
        // move constructor
        cache_probe(cache_probe&& neigh_cache_probe) = delete;
        // copy assignment
        cache_probe& operator=(const cache_probe& neigh_cache_probe) = delete;
        // move assignment
        cache_probe& operator=(cache_probe&& neigh_cache_probe) = delete;

        // generate a recommended buffer size
        // static member --> can be called without a class instance
        static std::size_t rec_buffer_size();

        // measure time of 1 load
        double measure_load();
};

#endif