#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <chrono>
#include <string_view>
#include <cstdlib>
#include <stdexcept>

constexpr std::size_t alignment = 64;
constexpr std::size_t cache_line_size = 64;
constexpr std::size_t jump_gap_bytes = (1 << 12);

template <typename T>
struct alignas(alignment) cache_line_node{
    T value{};
    cache_line_node* next{};
};

template <typename T>
std::vector<cache_line_node<T>> build_chain(std::size_t buffer_size, const std::string& run_mode){
    std::size_t buffer_len = buffer_size / cache_line_size;
    std::vector<cache_line_node<T>> chain(buffer_len, cache_line_node<T>());
    if (run_mode == "same_set"){
        // make each element 4096 bytes gap -- 64 elements gap
        // so that new num will fall into the same set as old num
        std::size_t gap_size = jump_gap_bytes / cache_line_size;
        std::vector<std::size_t> index_list{};
        for (std::size_t index = 0; index < buffer_len; index += gap_size){
            index_list.push_back(index);
        }
        for (std::size_t i = 0; i < index_list.size(); i ++){
            std::size_t cur_index = index_list[i];
            std::size_t next_index = (i + 1 == index_list.size())? 0 : index_list[i + 1];
            chain[cur_index].value = T();
            chain[cur_index].next = &chain[next_index];
        }
    }
    else if (run_mode == "packed"){
        std::size_t sweep = buffer_size / jump_gap_bytes;
        std::vector<std::size_t> index_list{};
        for (std::size_t index = 0; index < sweep; index ++){
            index_list.push_back(index);
        }
        for (std::size_t i = 0; i < index_list.size(); i ++){
            std::size_t cur_index = index_list[i];
            std::size_t next_index = (i + 1 == index_list.size())? 0 : index_list[i + 1];
            chain[cur_index].value = T();
            chain[cur_index].next = &chain[next_index];
        }
    }
    return chain;
}

template <typename T>
std::uint32_t count_steps_in_chain(const std::vector<cache_line_node<T>>& chain){
    std::uint32_t num_steps{};
    const cache_line_node<T>* cur_temp = &chain[0];
    while (num_steps < chain.size()){
        const cache_line_node<T>* next_node = cur_temp->next;
        num_steps ++;
        cur_temp = next_node;
        if (cur_temp == &chain[0]) break;
    }
    return num_steps;
}

template <typename T>
double chase_chain(const std::vector<cache_line_node<T>>& chain, std::uint32_t num_moves){
    std::size_t chain_size = chain.size();
    std::uint32_t num_steps = count_steps_in_chain(chain);
    if (num_steps != static_cast<std::uint32_t>(chain_size / (jump_gap_bytes / cache_line_size))){
        throw std::logic_error("chain is not a single cycle: " + std::to_string(num_steps)
                       + " steps, expected " + std::to_string(chain_size / (jump_gap_bytes / cache_line_size)));
    }
    auto start_time = std::chrono::steady_clock::now();
    const cache_line_node<T>* cur_temp = &chain[0];
    for (std::uint32_t move = 0; move < num_moves; move ++){
        const cache_line_node<T>* next_node = cur_temp->next;
        cur_temp = next_node;
    }
    auto end_time = std::chrono::steady_clock::now();
    if (cur_temp != &chain[0]){
        throw std::logic_error("Lap is not finished");
    }
    double elapsed_double = std::chrono::duration<double, std::nano>(end_time - start_time).count();
    double avg_time_per_move = elapsed_double / static_cast<double>(num_moves);
    return avg_time_per_move;
}

int main(int argc, char* argv[]){
    try{
        std::vector<std::size_t> buffer_size_list{};
        std::size_t base_size = 4096;
        std::size_t sweep = 1;
        while (sweep <= 24){
            buffer_size_list.push_back(base_size * sweep);
            sweep ++;
        }
        if (argc == 2 && std::string_view{argv[1]} == "--header"){
            std::cout << "run_mode,num_cache_lines,ns_per_load" << '\n';
            return EXIT_SUCCESS;
        }
        // 2 mode: same_set and packed
        // same_set = fall into same cache set
        // packed = fall into different cache sets
        /*
            compare: same len, but one list has all elements to compete
            for the same cache set, and the other one has all elements to 
            be spread out to multiple cache sets.
        */
        if (argc != 2){
            std::cerr << "Usage: ./l1_assoc <mode>" << '\n';
            return EXIT_FAILURE;
        }
        std::string run_mode{argv[1]};
        if (run_mode != "same_set" && run_mode != "packed"){
            std::cerr << "Run mode are same_set or packed" << '\n';
            return EXIT_FAILURE;
        }
        for (auto& buffer_size : buffer_size_list){
            std::vector<cache_line_node<std::uint32_t>> chain{build_chain<std::uint32_t>(buffer_size, run_mode)};
            std::size_t cur_sweep_size{buffer_size / base_size};
            std::size_t num_moves{cur_sweep_size * (16000000 / cur_sweep_size)};
            double time_per_move = chase_chain(chain, static_cast<std::uint32_t>(num_moves));
            std::cout << run_mode << ',' << cur_sweep_size << ',' << time_per_move << '\n';
        }
    }
    catch (const std::exception& e){
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}