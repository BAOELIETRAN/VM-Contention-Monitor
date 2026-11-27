#include "cache_probe.h"

// tsc helpers private to this file
// un-named namespace --> stuff inside is internal linkage
namespace{
    std::uint64_t tsc_start_helper(){
        _mm_lfence();
        std::uint64_t ticks = __rdtsc();
        _mm_lfence();
        return ticks;
    }
    std::uint64_t tsc_end_helper(unsigned int* aux){
        std::uint64_t ticks = __rdtscp(aux);
        _mm_lfence();
        return ticks;
    }
    // calib test length -- min # of nodes to test
    constexpr std::size_t g_min_calib_loads = 100000;
    // max # of nodes to test
    constexpr std::size_t g_max_calib_loads = 4000000;
    // target len of each call in ns
    constexpr std::size_t g_target_ns_per_call = 1000000;
    constexpr std::size_t g_min_loads_per_call = 1000;
    constexpr std::size_t g_max_loads_per_call = 2000000;
}

std::size_t cache_probe::count_steps_in_chain() const{
    std::size_t num_steps{};
    const cache_line_node* cur_temp = &m_buffer[0];
    while (num_steps < m_num_nodes){
        const cache_line_node* next_node = cur_temp->next;
        num_steps ++;
        cur_temp = next_node;
        if (cur_temp == &m_buffer[0]) break;
    }
    return num_steps;
}

// 1 load = 1 node visited
double cache_probe::chase_ns_per_load(std::size_t num_loads){
    unsigned int aux{};
    auto lap_start_time = tsc_start_helper();
    std::size_t counter{};
    while (counter < num_loads){
        cache_line_node* next_node = m_cur_node->next;
        m_cur_node = next_node;
        counter ++;
    }
    auto lap_end_time = tsc_end_helper(&aux);
    double lap_time = static_cast<double>(lap_end_time - lap_start_time) / m_num_tsc_ticks_per_ns;
    double time_per_load = lap_time / num_loads;
    return time_per_load;
}

cache_probe::cache_probe(std::size_t buffer_size){
    if (buffer_size < 4096 || buffer_size % g_cache_line_size != 0){
        throw std::invalid_argument("Invalid buffer size");
    }
    m_buffer_size = buffer_size;
    m_num_nodes = buffer_size / g_cache_line_size;
    m_buffer.resize(m_num_nodes);
    std::vector<std::size_t> index_list(m_num_nodes, 0);
    for (std::size_t index = 0; index < m_num_nodes; index ++){
        index_list[index] = index;
    }
    // pick a seed
    std::uint64_t seed{std::random_device{}()};
    // create a generator
    std::mt19937_64 generator{seed};
    // shuffle the index list
    std::shuffle(index_list.begin(), index_list.end(), generator);
    for (std::size_t index = 0; index < m_num_nodes; index ++){
        std::size_t cur_index = index_list[index];
        std::size_t next_index = index_list[(index + 1) % m_num_nodes];
        m_buffer[cur_index].next = &m_buffer[next_index];
    }
    // warm up lap
    std::size_t num_steps = count_steps_in_chain();
    if (num_steps != static_cast<std::size_t>(m_num_nodes)){
        throw std::logic_error("chain is not a single cycle: " + std::to_string(num_steps)
                       + " steps, expected " + std::to_string(m_num_nodes));
    }
    // calibrate the TSC frequency --> use to convert ticks to ns
    // how many TSC ticks per ns
    unsigned int aux{};
    auto clock_start = std::chrono::steady_clock::now();
    auto tsc_start = tsc_start_helper();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto clock_end = std::chrono::steady_clock::now();
    auto tsc_end = tsc_end_helper(&aux);
    m_num_tsc_ticks_per_ns = static_cast<double>(tsc_end - tsc_start) / std::chrono::duration<double, std::nano>(clock_end - clock_start).count();
    // # of nodes visited per call such that a call on a quiet machine can last ~ 1ms
    // time a test chase
    m_cur_node = &m_buffer[0];
    std::size_t calib_loads{std::clamp(m_num_nodes, g_min_calib_loads, g_max_calib_loads)};
    double cur_time_per_load = chase_ns_per_load(calib_loads);
    // cur_time_per_load can not be too small
    if (cur_time_per_load < 0.1){
        throw std::runtime_error("Invalid time per load");
    }
    double raw_loads = g_target_ns_per_call / cur_time_per_load;
    double rounded_loads = std::round(raw_loads / 1000.0) * 1000.0;
    std::size_t loads = static_cast<std::size_t>(rounded_loads);
    m_loads_per_call = std::clamp(loads, g_min_loads_per_call, g_max_loads_per_call);
}

std::size_t cache_probe::rec_buffer_size(){
    fs::path folder_path{"/sys/devices/system/cpu/cpu0/cache/"};
    std::size_t l3_cache_size{};
    bool index_founded{};
    bool size_founded{};
    for (const auto& sub_folder : fs::directory_iterator(folder_path)){
        if (index_founded && size_founded) break;
        if (sub_folder.is_directory()){
            std::string sub_folder_name{sub_folder.path().filename().string()};
            if (sub_folder_name.starts_with("index")){
                // open "level" directly by path (directory listing order is unspecified)
                fs::path level_path{sub_folder.path() / "level"};
                std::ifstream level_stream{level_path};
                // no readable "level" --> not the folder we want, try the next one
                if (!level_stream.is_open()) continue;
                std::string level_line{};
                std::getline(level_stream, level_line);
                if (level_line != "3") continue;
                index_founded = true;
                // this is the L3 folder: its "size" must be readable
                fs::path size_path{sub_folder.path() / "size"};
                std::ifstream file_stream{size_path};
                if (!file_stream.is_open()){
                    throw std::runtime_error(std::format("Can not open file {}", size_path.string()));
                }
                std::string line{};
                std::getline(file_stream, line);
                std::size_t index = 0;
                std::string num_string{};
                std::string suffix{};
                while (index < line.length()){
                    if (std::isdigit(static_cast<unsigned char>(line[index]))){
                        num_string.push_back(line[index]);
                        index ++;
                    }
                    else break;
                }
                while (index < line.length()){
                    suffix.push_back(line[index]);
                    index ++;
                }
                if (num_string.empty() || (!suffix.empty() && suffix != "M" && suffix != "K")){
                    throw std::runtime_error("Invalid cache size");
                }
                std::size_t multiply{};
                if (suffix == "K") multiply = 1024;
                else if (suffix == "M") multiply = 1024 * 1024;
                else if (suffix.empty()) multiply = 1;
                l3_cache_size = static_cast<std::size_t>(std::stoull(num_string) * multiply);
                size_founded = true;
            }
        }
    }
    if (!index_founded || !size_founded){
        throw std::runtime_error("Can not determine L3 cache size");
    }
    return l3_cache_size / 2;
}

double cache_probe::measure_load(){
    double cur_ns_per_load = chase_ns_per_load(m_loads_per_call);
    return cur_ns_per_load;
}