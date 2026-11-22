#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <chrono>
#include <random>
#include <string_view>
#include <cstdlib>
#include <utility>
#include <algorithm>
#include <stdexcept>

/*
    goal: measure how long CPU needs to wait when it reads 
    one piece of data from memory.
*/

// struct representing an element
// size = 64 bytes = 1 cache line
template <typename T>
struct alignas(64) cache_line_node{
    // inner data
    T value{};
    cache_line_node* next{};
};

/*
    @brief: build a chain of elements with random order
*/
template <typename T>
std::vector<cache_line_node<T>> build_chain(std::size_t buffer_size, const std::string& run_mode, std::size_t stride_len){
    static_assert(sizeof(cache_line_node<T>) <= 64, "The size of cache_line_node > 64!");
    std::size_t buffer_len{buffer_size / 64};
    std::vector<cache_line_node<T>> chain(buffer_len, cache_line_node<T>());
    if (run_mode == "random"){
        // pick a seed
        std::uint64_t seed{std::random_device{}()};
        // create a generator
        std::mt19937_64 generator{seed};
        // list of index
        std::vector<std::size_t> index_list(buffer_len, 0);
        for (std::size_t index = 0; index < buffer_len; index ++){
            index_list[index] = index;
        }
        // shuffle the index_list
        std::shuffle(index_list.begin(), index_list.end(), generator);
        // handle small loop: take index_list as the order for nodes
        for (std::size_t index = 0; index < buffer_len; index ++){
            std::size_t cur_node_idx = index_list[index];
            std::size_t next_node_idx = index_list[(index + 1) % buffer_len];
            chain[cur_node_idx].next = &chain[next_node_idx];
            chain[cur_node_idx].value = T();
        }
    }
    else if (run_mode == "sequential"){
        for (std::size_t index = 0; index < buffer_len; index ++){
            std::size_t next_node_idx = (index + 1) % buffer_len;
            chain[index].next = &chain[next_node_idx];
            chain[index].value = T();
        }
    }
    else if (run_mode == "stride"){
        std::size_t num_node_gap{stride_len / 64};
        if (num_node_gap < 1){
            throw std::invalid_argument("Not enough gap for striding");
        }
        std::vector<std::size_t> index_list(buffer_len, 0);
        std::size_t index = 0;
        // form the strided index list
        for (std::size_t cur_index = 0; cur_index < num_node_gap; cur_index ++){
            for (std::size_t next_index = cur_index; next_index < buffer_len; next_index += num_node_gap){
                index_list[index] = next_index;
                index ++;
            }
        }
        // build list from the index_list
        for (std::size_t index = 0; index < buffer_len; index ++){
            std::size_t cur_node_idx = index_list[index];
            std::size_t next_node_idx = index_list[(index + 1) % buffer_len];
            chain[cur_node_idx].next = &chain[next_node_idx];
            chain[cur_node_idx].value = T();
        }
    }   
    return chain;
}

/*
    @brief: count the number of steps to go from element 0 back to element 0
*/
template <typename T>
std::uint32_t count_steps_in_chain(const std::vector<cache_line_node<T>>& chain){
    std::uint32_t num_steps{};
    const cache_line_node<T>* cur_temp = &chain[0];
    while (true){
        const cache_line_node<T>* next_node = cur_temp->next;
        num_steps ++;
        cur_temp = next_node;
        if (cur_temp == &chain[0]) break;
    }
    return num_steps;
}

/*
    @brief: measure the avg time CPU waits for one cache line to arrive
    - start from element 0 --> num moves should ⋮ chain size
    - CPU has ele 0, then it has the pointer to ele 1, then CPU brings ele 1 to CPU.
    - --> the gap between ele 0 and ele 1 in the CPU = wait time to bring ele 1 to CPU.
    - wait time for each element is different --> average out.
*/
template <typename T>
double chase_chain(const std::vector<cache_line_node<T>>& chain, std::uint32_t num_moves){
    std::size_t chain_size = chain.size();
    // warm-up lap (not timed):
    // - fills the caches and TLB with THIS buffer's data, replacing leftovers from setup
    // - after it, every lap behaves the same, so the timed run measures normal reads
    // - also checks the chain visits every node exactly once
    // tldr: first run will have more cache misses --> need to warmup for fair profiling.
    std::uint32_t num_steps = count_steps_in_chain(chain);
    if (num_steps != static_cast<std::uint32_t>(chain_size)){
        throw std::logic_error("chain is not a single cycle: " + std::to_string(num_steps)
                       + " steps, expected " + std::to_string(chain_size));
    }
    // running a lot of moves --> make the overhead time become small
    // --> better reflect the time of each move (wait time for each element)
    auto start_time = std::chrono::steady_clock::now();
    const cache_line_node<T>* cur_temp = &chain[0];
    for (std::uint32_t move = 0; move < num_moves; move ++){
        const cache_line_node<T>* next_node = cur_temp->next;
        cur_temp = next_node;
    }
    auto end_time = std::chrono::steady_clock::now();
    // use cur_temp to avoid compiler optimization to delete the whole loop
    if (cur_temp != &chain[0]){
        throw std::logic_error("Lap is not finished");
    }
    double elapsed_double = std::chrono::duration<double, std::nano>(end_time - start_time).count();
    double avg_time_per_move = elapsed_double / static_cast<double>(num_moves);
    return avg_time_per_move;
}

int main(int argc, char* argv[]){
    // Usage: ./chase <mode>, with mode can be sequential, random, or stride
    try{
        // list of sizes for buffer
        std::vector<std::size_t> buffer_size_list{};
        // add buffer sizes into buffer_size_list
        // goal: make it fine-grained --> know where the latency jumps
        // --> can diagnose why
        std::size_t cur_size = 4096;
        while (cur_size <= 536870912){
            buffer_size_list.push_back(cur_size);
            cur_size *= 2;
        }
        if (argc == 2 && std::string_view{argv[1]} == "--header"){
            std::cout << "run_mode,buffer_size_bytes,ns_per_load" << '\n';
            return EXIT_SUCCESS;
        }
        if (argc != 2){
            std::cerr << "Usage: ./chase <mode>" << '\n';
            return EXIT_FAILURE;
        }
        // run mode: random, sequential, or stride
        std::string run_mode{argv[1]};
        if (run_mode != "random" && run_mode != "sequential" && run_mode != "stride"){
            throw std::logic_error("Usage: run_mode = random | sequential | stride");
        }
        // stride len = 4 KB --> take data from one page at a time
        std::size_t stride_len{4096};
        std::uint32_t num_moves{16777216};
        for (auto& buffer_size : buffer_size_list){
            std::vector<cache_line_node<std::int32_t>> chain{build_chain<std::int32_t>(buffer_size, run_mode, stride_len)};
            double time_per_move = chase_chain(chain, num_moves);
            std::cout << run_mode << ',' << buffer_size << ',' << time_per_move << '\n';
        }
    }
    catch(const std::exception& e){
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}