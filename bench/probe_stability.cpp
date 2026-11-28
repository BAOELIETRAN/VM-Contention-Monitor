#include "cache_probe.h"
#include <string_view>

void busy_wait(std::chrono::nanoseconds gap){
    auto deadline = std::chrono::steady_clock::now() + gap;
    while (std::chrono::steady_clock::now() < deadline){
        _mm_pause();
    }
}

int main(int argc, char* argv[]){
    try{
        if (argc == 2 && std::string_view{argv[1]} == "--header"){
            std::cout << "buffer_size_bytes,call,ns_per_load" << '\n';
            return EXIT_SUCCESS;
        }
        if (argc >= 3){
            std::cerr << "Usage: ./probe_stability <optional_buffer_size>" <<'\n';
            return EXIT_FAILURE;
        }
        std::size_t buffer_size = cache_probe::rec_buffer_size();
        if (argc == 2){
            std::string buffer_size_str{argv[1]};
            buffer_size = std::stoull(buffer_size_str);
        }
        // create a new cache probe
        cache_probe new_cache_probe{buffer_size};
        std::cerr << "TSC freq (GHz): " << new_cache_probe.get_tsc_freq() << '\n';
        std::cerr << "Num loads per call: " << new_cache_probe.get_num_loads_per_call() << '\n';
        for (std::size_t call = 1; call <= 100; call ++){
            double avg_ns_per_load = new_cache_probe.measure_load();
            std::cout << buffer_size << ',' << call << ',' << avg_ns_per_load << '\n'; 
            // gap 100ms between each call
            busy_wait(std::chrono::milliseconds(100));
        }
    }
    catch (const std::exception& e){
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}