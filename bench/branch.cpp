#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

/*
    Branch prediction benchmark: sum the elements below a threshold.

    Usage: ./branch.exe <variant> [reps]
        variant: rand-branch | sorted-branch | rand-branchless | sorted-branchless
        reps:    passes over the array (default 200)

    One variant per run, so perf counters belong to one variant only:
        perf stat -e cycles,instructions,branch-misses taskset -c 0 ./branch.exe rand-branch

    Design notes:
    - Values are 0..N-1 and the threshold is N/2, so on random data the branch
      is a 50/50 coin flip (worst case for the predictor). On sorted data it is
      "taken" for the first half and "not taken" for the second: easy to predict.
    - The random array is a shuffle of the sorted one, so both have exactly the
      same values and must produce the same sum (a correctness check).
    - The compiler (GCC) is told not to turn the `if` into a cmov ("if-conversion")
      and not to vectorize, otherwise the "branched" version is secretly branchless.
      Check with: objdump -d --no-show-raw-insn branch.exe | less  (look for jl/jge
      inside sum_branched, and cmov/setl inside sum_branchless).
    - The sum is printed so the compiler cannot delete the loops.

    Conclusion (to confirm with numbers in NOTEBOOK.md):
    - Branched, random: the predictor can't learn the pattern -> ~50% of branches
      mispredict -> pipeline flushes -> low IPC, slow.
    - Branched, sorted: the predictor learns it -> almost no misses -> fast.
    - Branchless: no jump to predict, so random and sorted run the same speed.
    *** Branchless is slower than branched when the branch is easy to predict,
        and faster when the branch is hard to predict.
*/

constexpr std::uint32_t num_elements = 1e6;
constexpr int threshold = num_elements/2;

// non-inline functions have fixed positions in memory
// --> need to jump to those funcs when calling them
// for branched, turn off compiler optimization so that it can stay branched
__attribute__((noinline, optimize("no-if-conversion", "no-if-conversion2", "no-tree-vectorize")))
std::uint64_t sum_branched(const std::vector<int>& vec){
    std::uint64_t sum{};
    for (auto& num : vec){
        if (num < threshold) sum += num;
    }
    return sum;
}

__attribute__((noinline, optimize("no-tree-vectorize")))
std::uint64_t sum_branchless(const std::vector<int>& vec){
    std::uint64_t sum{};
    for (auto& num : vec){
        sum += num * (num < threshold);
    }
    return sum;
}

int main(int argc, char* argv[]){
    // ./branch --header prints the CSV header that matches the row printed below
    if (argc == 2 && std::string_view{argv[1]} == "--header"){
        std::cout << "variant,reps,total_ms,ns_per_elem,sum" << '\n';
        return EXIT_SUCCESS;
    }
    if (argc != 3){
        std::cerr << "Not enough arguments" << '\n';
        return EXIT_FAILURE;
    }
    std::string variant{argv[1]};
    int reps{std::stoi(argv[2])};
    if (variant != "rand-branch" && variant != "sorted-branch" &&
        variant != "rand-branchless" && variant != "sorted-branchless"){
        std::cerr << "unknown variant: " << variant << '\n';
        return EXIT_FAILURE;
    }
    // initialize the sorted array
    std::vector<int> sorted_vec(num_elements, 0);
    for (std::size_t index = 0; index < num_elements; index ++){
        sorted_vec[index] = index;
    }
    // initialize the unsorted array by shuffling up the sorted one
    std::vector<int> rand_vec{sorted_vec};
    std::random_device rand_seed{};
    std::mt19937 engine{rand_seed()};
    std::shuffle(rand_vec.begin(), rand_vec.end(), engine);
    // check variant and time
    const std::vector<int>& vec = variant.starts_with("rand")? rand_vec : sorted_vec;
    const bool branchless = variant.ends_with("branchless");
    std::uint64_t sum = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < reps; i ++){
        sum += branchless? sum_branchless(vec) : sum_branched(vec);
    }
    auto end = std::chrono::steady_clock::now();
    double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
    double ns_per_elem = total_ns / (double(num_elements) * reps);
    // one CSV row: variant,reps,total_ms,ns_per_elem,sum
    // (keep in sync with the --header line above)
    std::cout << variant << ',' << reps << ',' << total_ns / 1e6 << ','
              << ns_per_elem << ',' << sum << '\n';
    return EXIT_SUCCESS;
}

/*
    result:
        rand-branch: 321.689 ms, 1.60845 ns/element, sum = 24999950000000
        sorted-branch: 19.9743 ms, 0.0998713 ns/element, sum = 24999950000000
        rand-branchless: 22.5998 ms, 0.112999 ns/element, sum = 24999950000000
        sorted-branchless: 22.5252 ms, 0.112626 ns/element, sum = 24999950000000
*/