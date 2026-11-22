Phase 1:
- WSL:
    - windows subsystem for linux
    - let users run a real linux kernel directly inside Windows
- Hypervisor:
    - a software (lie between hardware and operating system) that can create and run VMs by splitting the current computer's resources into multiple isolated environments. 
- WSL and shared VMs give misleading timing for a job:
    - WSL2:
        - run linux inside a hidden virtual machine in Windows.
        - --> Windows decides when anything is run.
        - hardware counters are limited.
        - everything is just a virtualization --> Windows decide everything + our run on WSL will have overhead.
    - Shared cloud VMs:
        - put strangers on the same cache and memory bandwidth, and hypervisor can take the current VM's CPU time.
        - --> fighting for resources.
    --> noisy numbers we can't explain + performance variability.
- perf:
    - perf-events = event-oriented observability tool --> help for performance monitor and troubleshooting.
        - hardware events
        - software events
        - ... (many more kinds of events)
    - commands types:
        - listing events
        - counting events
        - profiling
        - static tracing
        - dynamic tracing
    - need symbols and stack traces to get the most out of perf.
        - symbols map raw memory addresses to readable functions and variable names.
    - stack traces:
        - always compile with frame pointers.
        - without it, we will see incomplete stacks from perf_events.
        - frame pointer = CPU register storing address of the stack frame of the current function.
    - dynamic tracing:
        - harder to use.
        - can be enabled on a live system without restarting anything.
    - we can visualize data from perf using flame graph.
    - perf can collect hardware counters, which are registers in CPU tracking low-level hardware events during program execution.
        - e.g. # of instructions executed, clock ticks, # of instructions per cycle, ...
    - perf needs to have permission to read hardware counters.
        - error: <not supported>
        - fix: drop the sudo requirement:
            cat /proc/sys/kernel/perf_event_paranoid        # probably 3 or 4
            echo 'kernel.perf_event_paranoid=1' | sudo tee /etc/sysctl.d/99-perf.conf
            sudo sysctl --system
            perf stat -e cycles,instructions ls            # should now work without sudo
    - CPU has different types of cores:
        - P-cores: powerful cores for high-performance computing.
            - every p-cores have its own L2 cache.
        - E-cores (atom): low-power cores for energy efficiency and basic tasks.
            - all e-cores share a L2 cache.
    --> a task can be run on different cores at different time --> can pin a task so that the perf profile is static.
- Instruction-level parallelism:
    - a single CPU core.
    - instruction pipelining:
        - to execute an instruction, CPU needs to do some works:
            - fetching a chunk of machine code from memory.
            - decoding it and split into instructions.
            - executing those instructions.
            - writing the results back into registers.
        - --> this whole sequence is long and can take many cycles.
        - --> to hide latency, modern CPUs use pipelining = after the instruction passes through the first stage, CPUs will process the next one right away.
    - main performance indicator for CPU = cycles per instruction (CPI)
    - in a traditional pipeline, stages are overlapped, and a new instruction can finish every single clock cycle.
        --> 1 cycle per instruction --> base CPI = 1.
    - we can achieve CPI < 1 by adding more execution units (resources) into one single core --> more instructions per cycle.
        - e.g. one core has 3 ALUs, 2 floating point units, ...
        - this is cheaper than adding more additional core.\
        - superscalar.
    - superscalar processor only works if instructions are independent.
    - CPU has a buffer to keep upcoming instructions, as soon as the required data for the instruction is ready and a processing unit is free, CPU will run it.
    - pipelining increases the throughput of the system but not the latency as an instruction still needs that amount of steps to be finished.
    - pipeline hazard:
        - when the next instruction can not execute on the following clock cycle.
        - structural hazard = when 2 or more parts of instructions need the same part of CPU.
        - data hazard = when the current instruction needs to wait for an operand to be computed from some previous step.
        - control hazard = when a CPU can't tell which instructions it needs to execute next.
        - solution - pipeline stall:
            - if an instruction is congested, stop all other instructions coming after it until it is resolved.
            - structural hazard = wait until the execution unit is ready.
            - data hazard = wait until the required data is computed.
            - control hazard = flush the entire pipeline and start over.
    - branching:
        - CPU will start to execute the branch that it guesses to be more likely to be run.
            - in order to be able to guess, out of all branches it ran before, it computes statistics about them and recognizes pattern overtime.
        - wrong prediction = control hazard = flush the whole pipeline.
        - correct prediction = pay almost nothing.
        - volatile in C++ = require the compiler to generate instructions that read from and write to memory on every single access.
        - branch prediction can happen as we breaks instructions into stages and use pipelining on them (even though instructions are fetched in a sequence).
        - branch prediction process:
            - CPU uses pipelining to fetch instructions.
            - when the CPU fetches a conditional branch instruction (e.g. if (...)), it will use educated guess to guess whether that if condition is true or false:
                - true = fetch the instructions inside that if block into the pipeline and execute them.
                - false = fetch the instructions inside the else block into the pipeline and execute them.
            - when the conditional branch instruction is fully executed:
                - if the result is true --> big win.
                - if the result is false --> flush the whole pipeline and fetch + execute instructions in the correct block.
    - branchless programming:
        - e.g.
            ```
                int sum = 0;
                for (int num = 0; num < 100; num ++)
                    sum += (num < 50) * num;
            ```
        - or we can use ternary operator: sum += (num < 50)? num : 0;
        - we can use predication to avoid costly branch misprediction.
            - but the CPU needs to compute both the "if" and "else" outcomes before picking the winner.
            - usually, the double work wastes ALU cycles.
        - branch prediction has control hazard, and predication has data hazard.
    - latency = how many cycles to finish one instruction.
    - throughput = how many instructions, on average, can be executed per cycle.
    - modern CPUs are superscalar ==> multiple execution units on a single core.
        - --> to speed up the process, we can divide the work across those execution units.
        - ![alt text](image/cpu_1acc.png)
        - ![alt text](image/cpu_2acc.png)
- Here is how to use perf to show # of cycles, instructions, and branch misses on powerful core:
    ```
        perf stat -e cycles,instructions,branch-misses ./branch.exe
    ```
- branchless code is slower than branched code when the branch is easy to predict, and faster when the branch is harder to predict.
- __attribute__ is used in C++ to convey special information to compiler.
- with branchless, there is never a wrong guess --> never a flush. (as the computation for the condition is computed, and the result can be used for both cases -- e.g. sum += (num < 50) * num) 
- CPU frequency scaling:
    - OS scales CPU frequency up or down in order to improve performance or save performance.
    - CPUFreq is a sub-system in kernel that can dynamically adjust the frequency of the CPU while running, and it has 2 layers:
        - scaling governors = execute algorithms to compute the desired CPU frequency.
            - performance governor = run the CPU at max frequency.
            - ondemand = adjust the CPU frequency dynamically according to current load (jump straight to a frequency to handle the load).
            - conservative = like ondemand but more gradually (increase frequency slowly until the load is hanled).
        - scaling drivers = interact with CPU directly + apply the CPU freq requested from scaling governors.  
    - intel_pstate = a driver in Linux kernel for Intel CPUs that can adjust CPU's clock speed (frequency) and electrical votage in real time.
        - to prevent the CPU from entering turbo state = echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
- Pinning a process to specific CPU cores in Linux, use:
    - taskset -c <core-list> <command> (for a new program).
    - taskset -c <core-list> -p <PID> (for an already running process).
- source and bash:
    - source = run the program in the current shell process.
    - bash = a new shell process is spawned to run the program and close once its done.
- for running bash script, anything we add after the file name is an argument
    - file name is $0
    - --> arguments start from $1
- if we use relative path in bash script, it will be resolved to the directory we are currently running the script.
- import pandas as pd == giving the short nickname pd for pandas library.
- ubuntu blocks pip install system-wide --> use virtual environment to run python.
    - activate virtual env = source .venv/bin/activate
    - record what we installed = pip freeze > requirements.txt
- branch predictor training is not transferred across different CPU cores --> if a program is run on multiple cores --> the branch predictor training for it will be restarted on the new core --> add more runtime.
    - --> run a program on different nodes will cause overhead (e.g. take data about the program from a cache of this node to the cache of the other node) + new branch predictor training.
    - --> bad for performance.
    - --> pin the program on 1 core. (e.g. taskset -c core_id)
- to reduce performance variability and increase performance, set the scaling governor = performance governor --> run at max CPU frequency.
    - to see all governors that the machine support:
        ```
            cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors
        ```
    - to see what governor that the machine is using:
        ```
            cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
        ```
    - to set the performance governor to one core (core 0): 
        ```
            sudo cpupower -c 0 frequency-set -g performance
        ```
    - to undo it:
        ```
            sudo cpupower -c 0 frequency-set -g powersave
        ```
- with turbo, the core can run the program at a frequency that is higher than its normal speed range, but it needs to depend on the temperature --> not steady --> should turn turbo off.
    - turn turbo off:
        ```
            echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
        ```
    - turn turbo on:
        ```
            echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
        ```
- frequency = cycles per second | 1 Ghz = 1 billion cycles per second.
- as we turn off turbo, the CPU will run with the base frequency --> less # of cycles per second but steadier.
- with turbo on:
    - peak runtime = 1.6 ns per element
    - peak frequency = 4.7 Ghz = 4,700,000,000 cycles per sec
    - --> # of cycles per element = 1.6 x 10^-9 x 4.7 x 10^9 = 7.52 cycles per element
- with turbo off:
    - peak runtime = 3.3 ns per element
    - peak frequency = 2.2 Ghz = 2,200,000,000 cycles per sec
    - --> # of cycles per element = 3.3 x 10^-9 x 2.2 x 10^9 = 7.25 cycles per element
- --> # of cycles per element is roughly the same.
- --> same amount of work to be done, but each step is longer without turbo.
- --> turbo changes how long each cycle lasts, not the amount of work we need to finish a job.
- --> we want the steadier baseline for this project as it will help us detect the contentsion easier. (e.g. a sudden jump from a steady line)

Phase 2:
- cache:
    - write through = data is written to both the cache and RAM at the same time.
    - write back = data is written to cache first, then it would be written to RAM later.
- memory hierarchy:
    - register
    - L1 cache (private)
    - L2 cache (private/shared)
    - L3 cache (shared)
    - RAM
    - Disk
    - the layers that are closer to the CPU are faster but smaller in size.
- when the CPU needs to process data, it does not just pull it straight from RAM or from disk. Data will move through a series of caches (L3 --> L2 --> L1)
- here is the process of bringing data from the RAM:
    - CPU first check all of its caches and does not see the data in any of the cache --> cache miss.
        - look at L1 - miss.
        - ask L2 - miss.
        - ask L3 - miss
        - --> ask memory controller to read data from RAM.
        - (each level is asked only after the one above it misses)
    - Then, data (with locality) from RAM will be brought to L3 cache, then L2 cache, then L1.
    - Then, the data is sent from L1 cache into CPU registers --> ready to be used.
- here is how data is written back to RAM:
    - when CPU is done computing the data (or using the data without modifying it), it will write the value back to L1 cache and mark the data as "dirty".
    - "dirty" = the data has been changed, and the version in RAM is old.
    - when the L1 cache is filled up:
        - if the data is clean --> the controller would not waste time or memory bandwidth to bring it to L2 --> L1 will overwrite that data with the new data --> the old clean data is erased.
        - if the data is not clean --> kick to L2 cache, then kick to L3 cache (if L2 cache is full), and then kick to RAM (if L3 cache is full).
- metrics:  
    - latency = time delay between the exact moment that the CPU requests a piece of data to the moment that data arrives in the CPU register.
    - bandwidth = total amount of data that can flow through the physical wire between a memory layer and the CPU over one second.
        - L1 cache --> CPU = 1000 GB/s
        - RAM --> CPU = 100 GB/s
- if the size of the array is small (fit to cache 1) --> CPU can take the whole array and do computation.
- if the size is bigger than the cache --> it get split between caches and RAM --> CPU needs to spend more time to fetch before doing any computation.
- besides read and write, the data can be:
    - read only = its value is brought from L1 cache to CPU, but the data is still marked as "clean"
    - write only = the data is in L1 cache with address X (or being brought from RAM to L1 cache), CPU write back new data to address X and mark the data as "dirty"
- cache bypassing:
    - if we use non-temporal read/write, we are telling the CPU that we won't need the data we just accessed in the future.
    - --> data does not need to go through the cache hierarchhy.
    - for non-temporal writes:
        - data moves from CPU registers into the Write Combining Buffer (WCB), which can merge smaller writes into a bullet 64-byte block.
        - Once full, WCB flushes the block directly to RAM.
    - for non-temporal reads:
        - data is fetched from RAM into a temporary load buffer.
        - then the data from that load buffer will be handed straight to CPU registers.
    - bypassing the cache is good if we will not look at it again for a long time.
- cache line = smallest fundamental unit of data transfer between RAM and CPU.
    - standard cache line size is 64 bytes.
    - at one time, CPU will fetch block(s) of data with size of 64 bytes from RAM to CPU.
    - --> modern CPU fetch memory in chunks of 64 bytes.
    - ![alt text](cache_line.png)
    - in this image, we can see that for K = 1 --> 16, we will have same runtime, but when K > 16, the runtime reduces:
        - a cache line contains 64 bytes --> 16 ints.
        - for K = 1 --> 16, we need to access every cache line.
        - but for K = 32, we can skip the second cache line and access the data in the third cache line.
        - --> less cache line is brought to the cache --> better performance.
        - (same thing with K > 32)
- instruction-level parallelism:
    - in this code snippet:
        ```
            int steps = 256 * 1024 * 1024;
            int[] a = new int[2];
            // Loop 1
            for (int i=0; i<steps; i++) { a[0]++; a[0]++; }
            // Loop 2
            for (int i=0; i<steps; i++) { a[0]++; a[1]++; }
        ```
    - the performance of loop 2 is better than that of loop 1:
        - the whole array a is in cache (spatial locality)
        - in loop 1, a[0] will be handled in sequence (first add, then second add)
        - in loop 2, a[0] and a[1] will be handled at the same time (pipeline style).
- cache associativity:
    - direct mapped cache:
        - 1 memory chunk is assigned to a slot in the cache using index.
        - 2 memory chunks are mapped to the same slot --> they can not be stored simultaneously in the cache.
    - n-way set associative cache:
        - 1 memory chunk can be stored in any one of the N particular slots in the cache.
            - the cache is split into cache sets, with each set contains N slots.
            - a memory chunk will be assigned to a set, and it can use any slot inside that set.
    - fully associative cache:
        - 1 memory chunk can be stored in any slot in the cache.
    - less severe than others in terms of affecting performance.
- false cache line sharing:
    - cache coherence:
        - each CPU core will have its own L1 cache.
        - if core 1 and core 2 load the same data into their L1 caches:
            - core 1 modifies it --> copy of core 2 will be outdated.
            - --> core 1 will send an invalidate message to core 2 telling core 2 to drop that data.
    - granularity catch:
        - hardware track the data status by the 64-byte cache line.
        - --> core 1 and core 2 bring 1 copy each of a cache line to their L1 caches, then modify data inside those cache lines.
        - even though core 1 and core 2 handle different data on that cache line:
            - if core 1 modify the data --> the whole cache line in core 2 will be outdated.
    - consequence:
        - as core 2 needs to invalidate the whole cache line while the data its handling is not really outdated.
        - --> false sharing 
    - e.g.
        - in this code snippet:
            ```
                private static int[] s_counter = new int[1024];
                private void UpdateCounter(int position)
                {
                    for (int j = 0; j < 100000000; j++)
                    {
                        s_counter[position] = s_counter[position] + 3;
                    }
                }
            ```
        - run the code snippet on 4 different threads with 4 different params.
        - when param is 0,1,2,3 the runtime is 4.3 secs.
        - when param is 16,32,48,64 the runtime is 0.28 secs.
        - the reason is positions 0,1,2,3 are likely on the same cache line --> only one core can use it at a time.
        - but for bigger positions, data are on different cache lines --> each core can work on 1 cache line at a time.
- hardware is unpredictable --> variety in performance.
- lscpu --caches (or lscpu -C) shows a table of the CPU's cache information (e.g. cache level + other hardware architecture details).
    NAME ONE-SIZE ALL-SIZE WAYS TYPE        LEVEL  SETS PHY-LINE COHERENCY-SIZE
    L1d       48K     320K   12 Data            1    64        1             64
    L1i       64K     512K   16 Instruction     1    64        1             64
    L2       2.5M      14M   10 Unified         2  4096        1             64
    L3        12M      12M   12 Unified         3 16384        1             64
    - L1d = L1 data cache - storing data.
    - one size = size of one cache.
    - all size = size of all that cache combined.
    - sets and ways = a cache is divided into multiple sets, and in each sets there are multiple ways.
- spatial locality:
    - within one cache line:
        - if data x from the cache line A is requested, then CPU will bring all the nearby data inside that cache line into cache.
        - CPU also fetches the adjacent cache lines:
            - but this will only happen when the CPU sees us reading cache lines in a pattern. 
            - then, it will grab a few cache lines ahead of time -- but staying within a 4KB page boundary.
- to make a template struct have size of 64 bytes --> we use explicit padding + static_assert to check the size at compile time.
- alignas(64):
    - is a C++11 specifier that sets the alignment of an object to 64 bytes.
        - alignment:
            - a compile time constraint attached to a data type.
            - any object of this type must be placed at a memory address that ⋮ to its alignment.
    - the compiler will apply trailing padding:
        - it will add the padding to the current object so that the total size of the object is a multiple of the object's alignment.
        - as we set the alignment to 64 bytes --> if the object's size < 64 --> compiler will add trailing padding to make its size = 64.
    - as each instance has the size of 64 bytes --> each instance will be fit into 1 cache line.
    - sizeof() will return the size of object + any padding.
    - internal padding (between members):
        - the compiler add padding to each member so each individual member can start at an offset ⋮ to its alignment.
        - e.g.
            struct object{
                char a{};
                int b{};
                char c{};
            };
            - alignment of int = 4 --> needs to have offset ⋮ 4 --> add 3 bytes padding to a.
    - user-defined types (struct, union, class):
        - alignment of these types = largest alignment among its non-static data members.
        - e.g.
            struct object{
                char a{};
                int b{};
                char c{};
            };
            - as the largest alignment among all non-static members (a, b, and c) is 4 --> alignment of the struct is 4.
            - --> compiler needs to ensure that the size of the struct ⋮ 4.
            - as we already added 3 bytes to a, we will add 3 more bytes to c --> total size = 12 ⋮ 4.
- vector always stores its elements on heap -- and the elements are contiguous on the heap.
    - as each program is granted a virtual address space --> contiguous on virtual address space.
    - stack only stores data about the vector object itself.
- struct also has template instantiation -- compiler will create a real struct that matches with the type T the users define.
- if the buffer size is big, then data will be split between caches and RAM.
    - --> to measure wait time of CPU for an element --> need to measure time of running a lot of moves and average them.
- const pointer = const T* ptr --> can not modify pointed value.
- pointer to const = T* const ptr --> can not point to any other thing.
- the plot needs to show where the latency jumps --> need to add more sizes.
- $ in bash = tell the shell to evaluate the expression or replace a variable with its resulting value
- analysis of cache_phase.png:
    - as we can see in the graph, when the size of the buffer <= size of L1 cache, the buffer lies completely inside L1. The avg time to bring one element to CPU registers is stable with buffer size from 4KB to 32 KB.
    - but when the size of the buffer > size of L1 cache and <= size of L2 cache, there are jumps in avg time to bring one element to CPU registers.
    - when the size of the buffer is > size of L2 cache and <= size of L3 cache, the avg time rises pretty sharply. 
        - data needs to be brought from L3.
        - key idea: L3 cache is shared among cores --> it contains a variety of data, not just data of the buffer.
            --> the L3 cache contains less data of the buffer, and the rest of the buffer will be in RAM.
            --> the avg time to bring data into CPU will be slow.
        - besides, TLB is a cache of the page table:
            - size of TLB is small --> it can hold such amount of page address translations.
            - but buffer size grows big --> CPU needs to look at the page table more often to search for translation as TLB may not contain it.
            - --> make the avg time to bring data into CPU slow.
- prefetching:
    - it is good to prefetch data that is likely to be accessed next if its location can be predicted.
    - out-of-order execution:
        - CPU can look ahead in the code and execute later instructions if their operands are ready.
            - --> it can handle other instructions while the current one is stuck waiting, as long as the future instructions does not depend on the current stuck one.
        - e.g. (arr[x] is not in the cache, while y is already in the register)
            a = arr[x]
            b = a + 5
            c = y * 2
            d = c + 10
        - in-order CPU:
            - run instruction 1, but arr[x] is in RAM --> memory controller (integrated inside CPU) will fetch the RAM to take it, but it will take 200 cycles.
            - run instruction 2, which depends on the data of instruction 1 --> wait until data is ready.
            - after instruction 1 and 2 are finished, then instruction 3 is run, then instruction 4.
            - --> CPU's math units stay idle while waiting for data from RAM.
        - out-of-order CPU (hide latency):
            - run instr 1, and memory controller will fetch data from RAM.
            - run instr 2, CPU sees that it needs a, so CPU parks instr 2 in waiting area.
            - run instr 3, and y is already in register --> CPU executes instr 3 immediately.
            - then it runs instr 4.
            - when data is finally arrived from RAM, then CPU can finish instr 1 as well as wake up instr 2 and finish it.
    - implicit prefetching (hardware prefetcher):
        - a hardware prefetcher can detects our memory access pattern and prefetch data.
            - sad news: hardware prefetcher can only detect simple patterns.
        - e.g. 
            - if we access a[0], a[1], a[2], ... (iterating linearly through an array or stepping by a constant stride)
                - --> the hardware prefetcher will detect the pattern and automatically pull upcoming cache line into L1/L2 cache before the instructions even request them.
        - sequential prefetching = predict adjacent blocks.
            - e.g. when block B is accessed, the hardware will fetch blocks B + 1, B + 2, ...
            - usecase = lineary array scan.
        - stride prefetching = detect and predict any constan address gap between memory addresses.
            - track the address difference ("stride") between consecutive accesses.
                - if accesses are seperated by a distance S (A, A + S, A + 2S, ...)
                - --> the prefetcher will predict A + 3S next.
            - usecase = strided loop iteration.
    - explicit prefetching (software prefetching):
        - sometimes the access pattern is too irregular that hardware prefetcher can not predict.
        - if we know what address we will need to fetch in the future --> we can manually tell the CPU to fetch the cache line ahead of time (using __builtin_prefetch(&a[idx[i + 8]])).
- locality:
    - spatial locality = if we grab one item, we will likely need items sitting next to it soon.
    - temporal locality = if we use something right now, we will likely to use it again soon.
- cache probe = something to measure the cache.
- software is working with virtual address.
- hardware (CPU, RAM, ...) is working with real physical address.
- work flow:
    - when a software needs a data.
    - the CPU will need to find where that data on the physical memory.
    - first of all, the CPU will need to translate the virtual address of the data to physical address.
    - to do that, it will look up TLB:
        - if the data is mapped to a page that is already in TLB:
            - CPU will compute offsets to know the physical address of that data.
            - if the data is on cache --> CPU will use it.
            - if not --> CPU will need to fetch it from RAM and bring it to cache.
        - if the data is not in any page in TLB:
            - the CPU will look up the page table and get the translation for the physical page that the data lives in.
            - then, it uses the physical address of the page, and also updates the TLB, and compute offsets to know the physical address of the current data.
            - if the data is on cache --> CPU will use it.
            - if not --> CPU will need to fetch it from RAM and bring it to cache.
    - however, the size of TLB is limited, so if we have multiple data and those data are scattered across different pages, then the CPU needs to spend more time on page table to search for addresses --> more run time.
- size of a page = 4KB.
- analysis of cache_chase_all_mode.png:
    - as we can see, the time to bring one element to CPU would be smallest for sequential mode, then stride, and then biggest for random.
    - the reasoning is that if we access elements in order (e.g. sequentially or in stride), then the hardware prefetcher in the CPU can learn the access pattern, and then it will know what data to fetch even before we need it and bring it into cache.
    - --> thats why the time to load an element to CPU is smaller for sequential and stride.
    - but for our project, the purpose of the project is to identify whether any neighbor is contending for L3 cache:
        - since we are using probe to measure L3, we will want to set the size of buffer of the probe to <= the size of L3 cache.
        - and in the cache, we will run many laps --> data is accessed again and again (and we also need a warmup run to let every data to sit in the cache). 
        - if we use predictable pattern --> the CPU will know what data to fetch before hand --> it can bring it into cache and then CPU can access it from cache --> the load time would not change that much --> can not tell whether is there anyone using our cache.
        - but if we use random order --> then the CPU will not know what data to access next beforehand --> it needs to access data one by one.
            - if older data is kicked out of cache --> the load time will be suddenly higher as CPU needs to take it from RAM (no prefetching) --> a possibility that a neighbor VM is contending for L3.
        - --> thats why random order would be better for our probe.
    - cache probe is just one component of the whole project, and it can not speak for every cases (some cases the cache probe will be wrong).