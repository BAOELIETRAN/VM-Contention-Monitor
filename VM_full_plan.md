# VM Contention Monitor: Full Project Plan

Nov 7, 2026 · @Bao

## Overview

Goal: build a tool that detects, from inside a VM, when a co-tenant is stealing shared L3 cache or memory bandwidth, then fixes it with Intel CAT/MBA. Core version done by late March 2027, full version by May 2027, at about 10 hours a week starting mid-November 2026.

You learn and build at the same time. Every phase is a short run of steps, and each step is a **Learn** (the concept, what to search, and one or two short links) followed straight away by a **Do** (code or an experiment that uses it). The benchmarks you write while learning become the project's parts: your cache benchmark becomes the detector's cache probe, your bandwidth benchmark becomes an aggressor.

How to use this plan:

- Tick the `[ ]` boxes as you go. Each phase's exit check is the bar for moving on.
- Keep a lab notebook (one markdown file in the repo). Write down every experiment: setup, command, result, what surprised you. This becomes your write-up and your interview stories.
- Winter break (mid-Dec to mid-Jan) covers Phases 4–5, the setup- and measurement-heavy part. Push extra hours there if you can.
- Budget 30–50% slip. If a phase runs long, cut Phase 9, never the write-up.

| Phase | Weeks | Dates (approx.) | Deliverable |
| --- | --- | --- | --- |
| 1. Native Linux + how a CPU runs code | 1–2 | Nov 9 – Nov 22 | Branch and IPC benchmarks you can explain with perf |
| 2. Caches | 3–4 | Nov 23 – Dec 6 | Cache probe + plot of the memory hierarchy |
| 3. Bandwidth, NUMA, virtual memory | 5–6 | Dec 7 – Dec 20 | Bandwidth probe + three aggressors |
| 4. Virtualization | 7–8 | Dec 21 – Jan 3 | KVM host, two pinned VMs, stable measurements |
| 5. Reproduce interference | 9–11 | Jan 4 – Jan 24 | Graph: victim p99 vs aggressor intensity |
| 6. Detector | 12–15 | Jan 25 – Feb 21 | In-guest daemon with measured delay, accuracy, overhead |
| 7. Mitigation | 16–17 | Feb 22 – Mar 7 | Graph: p99 recovered with CAT/MBA |
| 8. GCP validation | 18–19 | Mar 8 – Mar 21 | Results reproduced on a GCP sole-tenant node |
| Write-up (core done) | 20 | Mar 22 – Mar 28 | README, graphs, resume bullets |
| 9. Cloud layer (optional) | 21–24 | Mar 29 – Apr 25 | Multi-node detection + rescheduling |

## Phase 1: Native Linux + how a CPU runs code (weeks 1–2)

Goal: a native Linux machine you measure on, and your first numbers you can fully explain.

**Step 1: Get a real Linux machine**

- **Learn:** why WSL2 and shared VMs give misleading timings; what `perf` measures.
- **Search:** `perf stat tutorial`, `perf one-liners`
- **Read:** [Brendan Gregg: perf examples](http://www.brendangregg.com/perf.html) (just the one-liners section)
- **Do:** dual-boot Ubuntu, or use a UMD lab machine or cheap cloud VM for now. Install `perf`. Repo skeleton: CMake + C++20/23, `bench/`, `daemon/`, `scripts/`, `results/`, `analysis/`, `NOTEBOOK.md`.

* [ ] `perf stat ls` prints cycles and instructions.

**Step 2: Pipelines, IPC, branch prediction**

- **Learn:** pipelining, out-of-order execution, IPC, what a branch mispredict costs.
- **Search:** `CPU pipeline IPC explained`, `branch misprediction sorted vs unsorted array`, `branchless programming cmov`
- **Read:** [Algorithmica: Instruction-Level Parallelism](https://en.algorithmica.org/hpc/pipelining/), [The Cost of Branching](https://en.algorithmica.org/hpc/pipelining/branching/), [Branchless Programming](https://en.algorithmica.org/hpc/pipelining/branchless/)
- **Do:** `bench/branch.cpp`: sum elements below a threshold on sorted vs random data; measure with `perf stat -e cycles,instructions,branch-misses`; then make it branchless and measure again.

* [ ] You can explain the speed gap with IPC and branch-miss numbers.

**Step 3: Frequency, turbo, and noise**

- **Learn:** frequency scaling and turbo; why the same code times differently run to run; pinning.
- **Search:** `linux cpu frequency scaling performance governor`, `disable turbo boost intel_pstate no_turbo`, `taskset pin process to core`
- **Read:** [ArchWiki: CPU frequency scaling](https://wiki.archlinux.org/title/CPU_frequency_scaling)
- **Do:** run the branch benchmark 20 times and plot the spread. Set the `performance` governor, pin to one core, turn turbo off if you can. Plot again.

* [ ] Spread shrinks visibly.

## Phase 2: Caches → the cache probe (weeks 3–4)

Goal: understand the cache hierarchy by measuring it, and finish with the project's first real component, the cache probe.

**Step 1: Cache basics**

- **Learn:** L1/L2/L3, 64-byte cache lines, hit vs miss latency, which caches are private and which are shared (L3 is shared: that's the whole project).
- **Search:** `CPU cache hierarchy latency numbers`, `pointer chasing benchmark random permutation`
- **Read:** [Algorithmica: Memory Latency](https://en.algorithmica.org/hpc/cpu-cache/latency/), [Gallery of Processor Cache Effects](https://igoro.com/archive/gallery-of-processor-cache-effects/)
- **Do:** `bench/chase.cpp`: link a buffer's elements in a random cycle, chase pointers, time ns per load. Sweep 4 KB to 512 MB; plot ns/load vs size.

* [ ] Steps at the L1, L2, L3 sizes, matched to your CPU's spec sheet.

**Step 2: Prefetchers and access patterns**

- **Learn:** hardware prefetchers; spatial vs temporal locality.
- **Search:** `hardware prefetcher sequential stride`, `spatial temporal locality`
- **Read:** [Algorithmica: Prefetching](https://en.algorithmica.org/hpc/cpu-cache/prefetching/)
- **Do:** run `chase.cpp` sequential and strided; compare `perf stat -e LLC-load-misses`.

* [ ] You can explain why the steps vanish when access is sequential, and why the probe must stay random.

**Step 3: Sets, associativity, eviction**

- **Learn:** how an address maps to a set; ways; eviction. L3 ways are exactly what Intel CAT partitions later.
- **Search:** `set associative cache explained`, `cache associativity power of two stride`
- **Read:** [Algorithmica: Cache Associativity](https://en.algorithmica.org/hpc/cpu-cache/associativity/)
- **Do:** touch addresses that map to the same L3 set; show misses jump once you exceed the number of ways.

* [ ] Associativity explained with your own graph.

**Step 4: Turn the benchmark into the cache probe**

- **Learn:** timing short intervals: `rdtscp` + `lfence` vs `steady_clock`.
- **Search:** `rdtscp lfence benchmark`, `timing short code sections intel`
- **Read:** [Intel white paper: How to Benchmark Code Execution Times](https://cis.temple.edu/~qzeng/cis3207-spring18/files/ia-32-ia-64-benchmark-code-execution-paper.pdf) (sections 1–3), [McCalpin: timing short code sections](https://sites.utexas.edu/jdm4372/2018/07/23/comments-on-timing-short-code-sections-on-intel-processors)
- **Do:** refactor into `daemon/cache_probe.{h,cpp}`: a \~1 ms randomized chase over a configurable buffer, returning ns/load.

* [ ] Stable within \~2% across 100 runs on a quiet machine.

**Step 5: Shared L3 contention on one machine**

- **Learn:** what happens when two cores share L3.
- **Search:** `shared last level cache contention multicore`, `noisy neighbor cache interference`
- **Read:** [Bubble-Up (Google)](https://research.google/pubs/bubble-up-increasing-utilization-in-modern-warehouse-scale-computers-via-sensible-co-locations/), abstract and intro only
- **Do:** probe on core 0, cache thrasher (random access over 2–4× L3) on core 1; plot probe latency with and without the thrasher.

* [ ] Your first interference graph.

## Phase 3: Bandwidth, NUMA, virtual memory → probes and aggressors (weeks 5–6)

Goal: understand the memory system below the caches; finish the bandwidth probe and the three aggressors.

**Step 1: DRAM and memory bandwidth**

- **Learn:** memory controllers and channels; bandwidth vs latency; why bandwidth saturates as cores are added.
- **Search:** `memory bandwidth vs latency`, `STREAM benchmark triad`, `memory level parallelism`
- **Read:** [STREAM FAQ](https://www.cs.virginia.edu/stream/ref.html), [Algorithmica: Memory-Level Parallelism](https://en.algorithmica.org/hpc/cpu-cache/mlp/), [Intel MLC](https://www.intel.com/content/www/us/en/developer/articles/tool/intelr-memory-latency-checker.html) (tool page)
- **Do:** `bench/stream.cpp` (copy/scale/add over arrays far larger than L3). Run 1, 2, 4, 8… threads; plot GB/s; compare with `mlc --max_bandwidth`.

* [ ] You can say where bandwidth saturates and why.

**Step 2: Turn it into the bandwidth probe**

- **Learn:** measuring bandwidth in a short burst without saturating the bus yourself.
- **Search:** `measure memory bandwidth short burst non-temporal stores`
- **Do:** `daemon/bw_probe.{h,cpp}`: a \~1 ms streaming burst returning GB/s.

* [ ] Stable within a few percent on a quiet machine.

**Step 3: Virtual memory, TLB, huge pages**

- **Learn:** page tables, the TLB, 4 KB vs 2 MB pages.
- **Search:** `TLB miss huge pages performance`, `transparent huge pages madvise`
- **Read:** [Algorithmica: Virtual Memory](https://en.algorithmica.org/hpc/external-memory/virtual/), [Algorithmica: Memory Paging](https://en.algorithmica.org/hpc/cpu-cache/paging/)
- **Do:** pointer chase with 4 KB vs 2 MB pages; measure `perf stat -e dTLB-load-misses`.

* [ ] Speedup explained with TLB miss counts.

**Step 4: NUMA**

- **Learn:** multi-socket machines; local vs remote memory.
- **Search:** `NUMA local vs remote memory latency`, `numactl membind cpunodebind`
- **Read:** [Drepper, What Every Programmer Should Know About Memory (LWN series)](https://lwn.net/Articles/250967/), only the NUMA part
- **Do:** on a 2-socket machine, run the chase with memory local vs remote. No access yet? Do it in Phase 4.

* [ ] Local vs remote latency measured.

**Step 5: Build the aggressors**

- **Learn:** what makes a workload cache-hostile vs bandwidth-hostile.
- **Search:** `LLC antagonist microbenchmark`, `memory bandwidth antagonist`
- **Read:** [Heracles paper](https://web.stanford.edu/group/mast/cgi-bin/drupal/system/files/2015.heracles.isca_.pdf), section 2 only (shared-resource interference)
- **Do:** three aggressors in `bench/`, each with a 0–100% intensity knob: **cache thrasher** (knob = buffer size), **bandwidth hog** (knob = threads), **realistic** (large sort or hash join).

* [ ] Thrasher slows the cache probe; hog slows the bandwidth probe.

## Phase 4: Virtualization → your own mini cloud (weeks 7–8)

Goal: one physical host you fully control, running a victim VM and an aggressor VM on the same socket, with stable measurements inside the VMs.

**Step 1: Get a server with RDT**

- **Learn:** Intel RDT: CAT (cache), MBA (bandwidth), CMT/MBM (monitoring); which CPUs have it (Xeon; AMD EPYC has its own resctrl support; most desktop chips don't).
- **Search:** `Intel RDT overview CAT MBA CMT`, `resctrl mount cat_l3 cpuinfo`
- **Read:** [Linux kernel docs: resctrl](https://kernel.org/doc/html/v5.11/x86/resctrl.html) (intro and "info directory" only), [intel-cmt-cat README](https://github.com/intel/intel-cmt-cat)
- **Do:** get the machine (UMD lab Xeon → rented monthly Xeon → GCP C3 bare metal for scripted runs only). Check `grep -oE 'cat_l3|mba|cqm_llc' /proc/cpuinfo | sort -u`, then mount resctrl.

* [ ] resctrl mounts and lists L3 and MB resources.

**Step 2: How a hypervisor maps VMs onto hardware**

- **Learn:** KVM/QEMU and libvirt; vCPUs are host threads; SMT siblings share L1/L2; NUMA binding.
- **Search:** `libvirt vcpupin emulatorpin numatune`, `KVM CPU pinning guide`, `lscpu -e SMT siblings`
- **Read:** [Red Hat: libvirt NUMA tuning](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/7/html/virtualization_tuning_and_optimization_guide/sect-virtualization_tuning_optimization_guide-numa-numa_and_libvirt)
- **Do:** install KVM/QEMU and libvirt; create victim and aggressor VMs (e.g. 4 vCPUs, 8 GB each); pin each to whole physical cores on the same socket, memory bound to that node.

* [ ] VMs share only L3 and memory bandwidth, not cores.

**Step 3: Remove host noise**

- **Learn:** `isolcpus`, `nohz_full`, IRQ affinity, `irqbalance`.
- **Search:** `isolcpus nohz_full irqaffinity`, `linux low latency tuning checklist`
- **Read:** [Red Hat: Low Latency Performance Tuning (PDF)](https://access.redhat.com/sites/default/files/attachments/201501-perf-brief-low-latency-tuning-rhel7-v1.pdf), checklist section; [pmu-tools](https://github.com/andikleen/pmu-tools) README for `toplev`
- **Do:** isolate VM cores, stop `irqbalance`, steer IRQs away, fix frequency. Install `perf`, `toplev`, Intel MLC, `pqos` on the host.

* [ ] Cache probe in the victim varies under \~2% across 10 runs with the aggressor off. If not, find out why first.

**Step 4: Checkpoint: interference across VMs**

- **Do:** cache probe in the victim VM, thrasher in the aggressor VM.

* [ ] Same interference as Phase 2, now crossing VM boundaries.

## Phase 5: Reproduce the interference on a real service (weeks 9–11)

Goal: prove with clean data that a co-tenant can wreck a real service's tail latency while the service's code stays the same. This phase produces your headline graph.

**Step 1: Tail latency, measured correctly**

- **Learn:** p50/p99/p99.9; why averages lie; open-loop vs closed-loop load; coordinated omission.
- **Search:** `coordinated omission`, `HdrHistogram`, `open loop load generator`
- **Read:** [Everything You Know About Latency Is Wrong](https://bravenewgeek.com/everything-you-know-about-latency-is-wrong/) (short summary of Gil Tene's talk), then the [talk slides](https://qconsf.com/sf2012/dl/qcon-sanfran-2012/slides/GilTene_HowNotToMeasureLatency.pdf) if you want more; [HdrHistogram](https://hdrhistogram.github.io/HdrHistogram); [memtier\_benchmark](https://github.com/RedisLabs/memtier_benchmark) (see `--rate-limiting`)
- **Do:** pick the victim (Redis/memcached, or your own small C++ key-value server with a working set that fits in L3). Drive it at a fixed rate; record every latency into an HdrHistogram.

* [ ] Baseline: victim alone, 10 runs, p50/p99/p99.9 with spread.

**Step 2: Experiment design**

- **Learn:** one variable at a time, repeated runs, report spread, keep ground truth.
- **Search:** `benchmarking methodology variance repeated runs`
- **Read:** [Heracles paper](https://web.stanford.edu/group/mast/cgi-bin/drupal/system/files/2015.heracles.isca_.pdf), section 3 (characterization), as an example of how pros run these sweeps
- **Do:** `scripts/sweep.sh`: every aggressor × every intensity, 5–10 trials, raw CSV out.

* [ ] One command reproduces the whole sweep.

**Step 3: Host-side ground truth**

- **Learn:** CMT (L3 occupancy) and MBM (bandwidth) per group via `pqos -m`.
- **Search:** `pqos monitoring llc occupancy mbm`, `perf stat -e LLC-load-misses per pid`
- **Read:** [pqos man page](https://manpages.ubuntu.com/manpages/noble/man8/pqos.8.html)
- **Do:** at every sweep point, record each VM's L3 occupancy, bandwidth, and LLC misses from the host.

* [ ] You can show *why* the victim slowed: its L3 occupancy falls and misses rise.

**Step 4: The headline graph**

- **Learn:** plotting distributions honestly (error bars, log scale).
- **Search:** `pandas groupby quantile`, `matplotlib errorbar log scale`
- **Do:** plot victim p99 vs aggressor intensity, one line per aggressor, with host counters beside it; write a one-page explanation in `NOTEBOOK.md`.

* [ ] p99 rises by a clear factor (aim for 2× or more). This alone is a strong resume bullet.

## Phase 6: The detector (weeks 12–15)

Goal: a C++ daemon inside the victim VM that answers "is a neighbor hurting me right now, and how?" with no host access, checked against the Phase 5 ground truth.

**Step 1: Reading hardware counters from your own code**

- **Learn:** `perf_event_open`; counting vs sampling; the guest PMU in KVM; steal time.
- **Search:** `perf_event_open example read counter`, `KVM guest PMU -cpu host`, `CPU steal time /proc/stat`
- **Read:** [perf\_event\_open(2) man page](https://www.man7.org/linux/man-pages/man2/perf_event_open.2.html) (copy the example at the bottom); [CPI2 paper (Google)](https://storage.googleapis.com/gweb-research2023-media/pubtools/pdf/40737.pdf), sections 1–3, the closest thing to what you're building; [Red Hat: steal time](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/6/html/virtualization_host_configuration_and_guest_installation_guide/form-virtualization_host_configuration_and_guest_installation_guide-kvm_guest_timing_management-steal_time_accounting)
- **Do:** enable the guest PMU; write `daemon/counters.{h,cpp}` reading IPC and LLC misses/sec for the victim process; read steal time.

* [ ] Guest counter values roughly track what the host sees.

**Step 2: Assemble the daemon**

- **Learn:** low-overhead daemon design: one pinned thread, fixed duty cycle, no allocation in the loop.
- **Search:** `pthread_setaffinity_np example`, `SPSC ring buffer C++`
- **Do:** combine `cache_probe`, `bw_probe`, `counters` into `daemon/main.cpp`; each probe runs \~1 ms every \~100 ms; signals go into a ring buffer.

* [ ] Runs for an hour with no drift or leaks.

**Step 3: Detecting change**

- **Learn:** baselines (rolling median + MAD), EWMA, CUSUM; precision vs recall.
- **Search:** `CUSUM change detection python`, `median absolute deviation outlier detection`
- **Read:** [Wikipedia: CUSUM](https://en.wikipedia.org/wiki/CUSUM), [detect\_cusum notebook](https://nbviewer.org/format/script/github/BMClab/BMC/blob/master/notebooks/detect_cusum.ipynb) (prototype in Python first, then port to C++)
- **Do:** learn a baseline in quiet periods, flag departures, classify: *cache contention*, *bandwidth contention*, or *my own load changed* (app counters moved, probes didn't).

* [ ] Correct classification on hand-made test traces.

**Step 4: Evaluate like a real system**

- **Learn:** scoring a detector against ground truth; detection-delay distributions.
- **Search:** `change point detection evaluation detection delay false alarm`
- **Do:** 1–2 hour scenarios with aggressors switching on and off at random; host logs true times. Measure delay (p50/p99), precision, recall, overhead (victim p99 detector on vs off; target under 1–2%). Sweep probe frequency to get the overhead vs delay curve.

* [ ] Results table plus an honest note on where it fails.

**Step 5: Export**

- **Learn:** the Prometheus text format.
- **Read:** [Prometheus exposition formats](https://prometheus.io/docs/instrumenting/exposition_formats/)
- **Do:** `/metrics` endpoint, JSON event log, Grafana panel of probe signals and alarms.

* [ ] Dashboard shows an alarm within seconds of starting an aggressor.

## Phase 7: Mitigation with Intel CAT/MBA (weeks 16–17)

Goal: when the detector fires, the host fences off the aggressor and the victim's p99 recovers.

**Step 1: Cache partitioning (CAT)**

- **Learn:** CAT way masks; how ways map to capacity (ties back to Phase 2, Step 3); resctrl groups and the `schemata` file.
- **Search:** `resctrl schemata L3 mask example`, `Intel CAT cache ways bitmask`
- **Read:** [Linux kernel docs: resctrl](https://kernel.org/doc/html/v5.11/x86/resctrl.html), the "Resource alloc and monitor groups" and "schemata" sections; [Intel RDT hands-on lab slides](https://www.slideshare.net/slideshow/intel-rdt-handson-lab/77121812)
- **Do:** host controller that creates one resctrl group per VM and moves each VM's vCPU threads into it. Give the victim 2, 4, 6… ways; plot victim p99 for each.

* [ ] Graph: victim p99 vs ways reserved, under the thrasher.

**Step 2: Bandwidth throttling (MBA)**

- **Learn:** how MBA caps bandwidth, and how coarse it is in practice.
- **Search:** `memory bandwidth allocation MBA granularity`, `resctrl MB schemata`
- **Read:** same kernel doc, "Memory bandwidth allocation" section
- **Do:** cap the aggressor at 10…100% under the bandwidth hog; plot victim p99 and aggressor throughput.

* [ ] Graph: victim gain vs aggressor loss.

**Step 3: Close the loop**

- **Learn:** control-loop basics: hysteresis, so the policy doesn't flap on and off.
- **Search:** `feedback controller hysteresis`
- **Read:** [Heracles paper](https://web.stanford.edu/group/mast/cgi-bin/drupal/system/files/2015.heracles.isca_.pdf), section 4 (the controller)
- **Do:** detector signals the host (socket or HTTP); host applies CAT/MBA and relaxes it when contention ends. Compare none / CAT / MBA / CAT + MBA on the same aggressor schedule.

* [ ] Graph: victim p99 under each policy, plus the aggressor's lost throughput. This is your "reduced p99 by X%" bullet.

## Phase 8: Validate on GCP (weeks 18–19)

Goal: show the detector works on a real cloud hypervisor, not just your own KVM setup.

**Step 1: Infrastructure as code**

- **Learn:** Terraform basics; GCP projects, quotas, billing alerts.
- **Search:** `terraform google_compute_instance quickstart`, `gcp budget alert`
- **Read:** [GCP: Create a VM using Terraform](https://docs.cloud.google.com/docs/terraform/get-started-with-terraform)
- **Do:** set a budget alert first; write Terraform that creates one small VM and destroys it.

* [ ] One command up, one command down, nothing left running.

**Step 2: Sole-tenant nodes**

- **Learn:** node templates, node groups, placing VMs on a specific physical host.
- **Search:** `gcp sole tenant node terraform`, `google_compute_node_group`
- **Read:** [GCP: Sole-tenant nodes](https://docs.cloud.google.com/compute/docs/nodes/sole-tenant-nodes)
- **Do:** request quota if needed; Terraform a sole-tenant node with victim and aggressor VMs on it; run the Phase 6 scenario; copy results to a bucket; destroy.

* [ ] Both VMs report the same physical server ID.

**Step 3: Compare lab vs cloud**

- **Learn:** what Google's hypervisor exposes to guests and what it hides; why a guest can't apply CAT.
- **Search:** `gcp c4 performance monitoring unit vm`, `gcp c3 bare metal`
- **Read:** [GCP: Bare metal instances](https://docs.cloud.google.com/compute/docs/instances/bare-metal-instances), [Intel: PMU on Google Cloud (C3 metal, C4)](https://www.intel.com/content/www/us/en/developer/articles/technical/vtune-profiler-functionality-on-google-cloud.html)
- **Do:** re-run the detection evaluation; table of KVM lab vs GCP (delay, precision, recall, overhead). Optional: one scripted C3 bare-metal run to check CAT/MBA.

* [ ] Comparison table with each difference explained. On GCP, mitigation becomes "alert or move the workload."

## Phase 9 (optional): Multi-node cloud layer (weeks 21–24)

Goal: detectors on many nodes, one central view, automatic moves off contended machines. Skip if earlier phases run late.

**Step 1: Package for a cluster**

- **Learn:** Kubernetes DaemonSets.
- **Search:** `kubernetes daemonset node agent example`
- **Read:** [Kubernetes: DaemonSet](https://kubernetes.io/docs/concepts/workloads/controllers/daemonset)
- **Do:** containerize the detector; run it as a DaemonSet on a small GKE cluster (3–5 cheap nodes).

* [ ] Every node exports metrics.

**Step 2: Central view**

- **Learn:** Prometheus scraping and alert rules.
- **Search:** `prometheus alerting rules example`, `grafana dashboard per node`
- **Do:** one Grafana dashboard: probe signals, contention events, victim p99 per node.

* [ ] One screen shows the whole fleet.

**Step 3: Automatic rescheduling**

- **Learn:** taints, tolerations, pod eviction.
- **Search:** `kubectl taint node NoExecute evict pod`
- **Read:** [Kubernetes: Taints and Tolerations](https://kubernetes.io/docs/concepts/scheduling-eviction/taint-and-toleration/)
- **Do:** a small controller that taints a node under sustained contention and moves the latency-sensitive pod elsewhere; measure p99 before and after.

* [ ] Measured p99 improvement from a move.

**Step 4: In the wild**

- **Learn:** an observation isn't a proof (no ground truth on shared VMs).
- **Do:** run passively on ordinary shared VMs for 1–2 weeks; log every episode.

* [ ] Short report: N nodes, N weeks, N episodes, p99 impact, labeled as observations.

## Write-up, repo, resume, interview prep (week 20)

The write-up matters as much as the code. Interviewers will read the README and look at your graphs, not your source files.

- [ ] README: the problem in 3 sentences, an architecture picture (victim VM, aggressor VM, detector, host controller), how to reproduce with one script.
- [ ] Four graphs: (1) victim p99 vs aggressor intensity, (2) detection timeline with true vs detected events, (3) overhead vs detection-delay trade-off, (4) p99 under the four mitigation policies.
- [ ] "What surprised me" section: 3–5 things that didn't behave as expected and what you found. These are your best interview stories.
- [ ] Limitations section, written honestly.
- [ ] Resume bullets: fill the \[X\] placeholders in the draft bullets with your real numbers.

**Interview prep: be ready to answer these cold:**

1. Why does a co-tenant on a different core slow you down at all?
2. How did you know your probe measured contention and not your own load?
3. Why randomize the pointer chase? What does the prefetcher do otherwise?
4. What was your biggest source of measurement noise, and how did you remove it?
5. Why p99 and not average latency? What is coordinated omission?
6. CAT vs MBA: when does each help, and when does neither?
7. Your probe uses the cache too. How did you keep the detector from becoming a noisy neighbor?
8. What would change on a real trading server with no VMs?

## Resources

No books needed. Every step above already has its search terms and one or two short links. These are the sites you'll keep coming back to.

| Site | Use it for | Phases |
| --- | --- | --- |
| [Algorithmica: Algorithms for Modern Hardware](https://en.algorithmica.org/hpc/) | Short pages with C++ benchmarks for pipelines, caches, prefetching, associativity, TLB | 1–3 |
| [Brendan Gregg: perf examples](http://www.brendangregg.com/perf.html) | Copy-paste `perf` one-liners | All |
| [Linux kernel docs: resctrl](https://kernel.org/doc/html/v5.11/x86/resctrl.html) | CAT, MBA, monitoring files | 4, 7 |
| [intel-cmt-cat (pqos)](https://github.com/intel/intel-cmt-cat) | RDT monitoring and control tool | 4, 5, 7 |
| [pmu-tools (toplev)](https://github.com/andikleen/pmu-tools) | "Why is this slow?" in one command | 4–6 |
| [Intel MLC](https://www.intel.com/content/www/us/en/developer/articles/tool/intelr-memory-latency-checker.html) | Reference latency and bandwidth numbers | 3, 4 |
| [perf\_event\_open(2)](https://www.man7.org/linux/man-pages/man2/perf_event_open.2.html) | Reading counters from your own C++ | 6 |
| [CPI2 paper](https://storage.googleapis.com/gweb-research2023-media/pubtools/pdf/40737.pdf) and [Heracles paper](https://web.stanford.edu/group/mast/cgi-bin/drupal/system/files/2015.heracles.isca_.pdf) | How Google solved this; only the sections named in each step | 3, 5–7 |
| [Everything You Know About Latency Is Wrong](https://bravenewgeek.com/everything-you-know-about-latency-is-wrong/) | Measuring tail latency correctly | 5 |
| [GCP sole-tenant nodes](https://docs.cloud.google.com/compute/docs/nodes/sole-tenant-nodes), [GCP bare metal](https://docs.cloud.google.com/compute/docs/instances/bare-metal-instances) | Cloud setup | 8 |

Optional, only if you want depth later: [Bakhvalov's free perf book](https://github.com/dendibakh/perf-book) and [Drepper's memory paper](https://lwn.net/Articles/250967/). Use them as references to search inside, not to read cover to cover.

All links checked on Nov 9, 2026.

## Pitfalls, cost control, rules

**Pitfalls that will cost you weeks:**

- **Measuring in WSL2 or a shared VM during development.** Numbers will be noisy and partly meaningless. Use native Linux from Phase 1.
- **Turbo and frequency scaling.** Turbo makes "slower" results look like contention. Fix the frequency before trusting any number.
- **SMT siblings.** If victim and aggressor share a physical core, you're measuring L1/L2 contention, not L3. Give each VM whole cores.
- **Prefetchers.** A sequential or strided probe gets hidden by the prefetcher. Always randomize.
- **Averages.** Report p99/p99.9 with spread over repeated runs, never a single average.
- **Coordinated omission.** A closed-loop load generator hides latency spikes. Use open-loop at a fixed rate.
- **Scope creep.** Don't add a GUI, a web app or ML before Phase 7 works.

**Cost control:**

- Develop on a cheap or borrowed machine. Use GCP sole-tenant and bare metal only for scripted runs, then destroy them.
- Set a GCP budget alert on day one. Use `terraform destroy` at the end of every session; check the console for leftovers.
- Check UMD and Google for student cloud credits before paying.

**Rules:**

- Only run aggressors against your own VMs and hosts. Degrading other tenants on shared cloud hardware breaks provider terms. Passive detection on shared VMs is fine.
- Commit raw data with every graph, plus the exact command that made it. Results you can't reproduce will fall apart under interview questions.
