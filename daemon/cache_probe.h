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
#include <cctype>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

constexpr std::size_t g_alignment = 64;
constexpr std::size_t g_cache_line_size = 64;

/*
    caller will call cache_probe every 100ms.
    caller will decide the buffer size.
    no warm-up between calls.
*/
class cache_probe{
    private:
        struct alignas(g_alignment) cache_line_node{
            cache_line_node* next{};
        };
        std::size_t m_buffer_size{};
        std::size_t m_num_nodes{};
        std::vector<cache_line_node> m_buffer{};
        // how many loads per call should the host make to fit 1ms
        std::size_t m_loads_per_call{};
        double m_num_tsc_ticks_per_ns{};
        // store the last position in the buffer
        cache_line_node* m_cur_node{};
        // @brief: count the number of steps to go from element 0 back to element 0
        std::size_t count_steps_in_chain() const;
        // @brief: helper to time each load on avg
        double chase_ns_per_load(std::size_t num_loads);
    public:
        // default constuctor
        cache_probe() = delete;
        // parameterized constructor
        explicit cache_probe(std::size_t buffer_size);
        // copy constructor
        cache_probe(const cache_probe& neigh_cache_probe) = delete;
        // move constructor
        cache_probe(cache_probe&& neigh_cache_probe) = delete;
        // copy assignment
        cache_probe& operator=(const cache_probe& neigh_cache_probe) = delete;
        // move assignment
        cache_probe& operator=(cache_probe&& neigh_cache_probe) = delete;

        // getters
        std::size_t get_buffer_size() const{ return m_buffer_size; }

        std::size_t get_num_loads_per_call() const{ return m_loads_per_call; }        

        double get_tsc_freq() const{ return m_num_tsc_ticks_per_ns; }

        /*
            @brief: generate a recommended buffer size
                    static member --> can be called without a class instance
        */
        static std::size_t rec_buffer_size();

        /*
            @brief: measure avg time of a load
            tldr: 
                - continue the random chase from wherever
                the last call stopped.
                - does exactly m_loads_per_call loads, which
                takes about 1ms on a quiet machine
                - return avg ns per load for the chase.
        */
        double measure_load();
};

#endif