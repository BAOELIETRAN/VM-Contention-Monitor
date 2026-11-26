#include "cache_probe.h"

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
    m_cur_node = &m_buffer[0];
    // warm up lap
    while (true){
        cache_line_node* next_node = m_cur_node->next;
        m_cur_node = next_node;
        if (m_cur_node == &m_buffer[0]) break;
    }
    // calibrate the TSC frequency --> use to convert ticks to ns
    // how many TSC ticks per ns
    unsigned int aux{};
    auto clock_start = std::chrono::steady_clock::now();
    auto tsc_start = __rdtsc();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto clock_end = std::chrono::steady_clock::now();
    auto tsc_end = __rdtscp(&aux);
    m_num_tsc_ticks_per_ns = static_cast<double>(tsc_end - tsc_start) / std::chrono::duration<double, std::nano>(clock_end - clock_start).count();
    // # of nodes visited per call such that a call on a quite machine can last ~ 1ms
    // time the lap
    auto lap_start_time = std::chrono::steady_clock::now();
    m_cur_node = &m_buffer[0];
    while (true){
        cache_line_node* next_node = m_cur_node->next;
        m_cur_node = next_node;
        if (m_cur_node == &m_buffer[0]) break;
    }
    auto lap_end_time = std::chrono::steady_clock::now();
    double lap_time = std::chrono::duration<double, std::nano>(lap_end_time - lap_start_time).count();
    double time_per_load = lap_time / m_num_nodes;
    m_loads_per_call = static_cast<std::size_t>(1000000 / time_per_load);
    m_cur_node = &m_buffer[0];
}

std::size_t cache_probe::rec_buffer_size(){
    fs::path folder_path{"/sys/devices/system/cpu/cpu0/cache/"};
    std::size_t l3_cache_size{};
    try{
        bool index_founded{};
        bool size_founded{};
        for (const auto& sub_folder : fs::directory_iterator(folder_path)){
            if (index_founded && size_founded) break;
            if (sub_folder.is_directory()){
                std::string sub_folder_name{sub_folder.path().filename().string()};
                if (sub_folder_name.starts_with("index")){
                    // check for file "level"
                    for (const auto& file : fs::directory_iterator(sub_folder)){
                        if (size_founded) break;
                        if (file.is_regular_file()){
                            std::string file_name{file.path().filename().string()};
                            if (file_name == "level"){
                                std::fstream file_stream(file.path(), std::ios::in);
                                if (!file_stream.is_open()){
                                    throw std::runtime_error(std::format("Can not open file {}", file_name));
                                }
                                std::string line{};
                                std::getline(file_stream, line);
                                if (line == "3") index_founded = true;
                            }
                            if (index_founded && file_name == "size"){
                                std::fstream file_stream{file.path(), std::ios::in};
                                if (!file_stream.is_open()){
                                    throw std::runtime_error(std::format("Can not open file {}", file_name));
                                }
                                std::string line{};
                                std::getline(file_stream, line);
                                l3_cache_size = static_cast<std::size_t>(std::stoi(line.substr(0, line.length() - 1)) * 1024);
                                size_founded = true;
                            }
                        }
                    }
                }
            }
        }
        if (!index_founded || !size_founded){
            throw std::runtime_error("Can not determine L3 cache size");
        }
    }
    catch (const std::exception& e){
        std::cerr << "File system error: " << e.what() << '\n';
    }
    return l3_cache_size / 2;
}