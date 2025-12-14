#include <iostream>
#include <fstream>
#include <queue>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <string>
#include "../../hnswlib/hnswlib.h"
#include "../../hnswlib/evolution.h"
#include "../../hnswlib/zswap.h"

#include <unordered_set>

// Simple JSON parser for ZswapConfig
#include <sstream>
#include <regex>

using namespace std;

// Simple function to parse JSON config file
hnswlib::ZswapConfig loadZswapConfigFromJson(const std::string& filename) {
    hnswlib::ZswapConfig config = {hnswlib::ZpoolType::ZBUD, hnswlib::MaxPoolPercent::_20, hnswlib::CompressorType::LZO, hnswlib::ShrinkerEnabled::YES, hnswlib::Enabled::YES};

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open zswap config file: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_content = buffer.str();

    // Simple regex-based JSON parsing for the specific format
    std::regex max_pool_regex("\"max_pool_percent\"\\s*:\\s*(\\d+)");
    std::regex compressor_regex("\"compressor\"\\s*:\\s*\"([^\"]+)\"");
    std::regex zpool_regex("\"zpool\"\\s*:\\s*\"([^\"]+)\"");
    std::regex shrinker_regex("\"shrinker_enabled\"\\s*:\\s*(true|false)");
    std::regex enabled_regex("\"enabled\"\\s*:\\s*(true|false)");

    std::smatch match;
    if (std::regex_search(json_content, match, max_pool_regex)) {
        config.max_pool_percent = hnswlib::stringToMaxPoolPercent(match[1].str());
    }
    if (std::regex_search(json_content, match, compressor_regex)) {
        config.compressor = hnswlib::stringToCompressorType(match[1].str());
    }
    if (std::regex_search(json_content, match, zpool_regex)) {
        config.zpool = hnswlib::stringToZpoolType(match[1].str());
    }
    if (std::regex_search(json_content, match, shrinker_regex)) {
        config.shrinker_enabled = (match[1].str() == "true") ? hnswlib::ShrinkerEnabled::YES : hnswlib::ShrinkerEnabled::NO;
    }
    if (std::regex_search(json_content, match, enabled_regex)) {
        config.enabled = (match[1].str() == "true") ? hnswlib::Enabled::YES : hnswlib::Enabled::NO;
    }

    return config;
}
using namespace hnswlib;

class StopW {
    std::chrono::steady_clock::time_point time_begin;
 public:
    StopW() {
        time_begin = std::chrono::steady_clock::now();
    }

    float getElapsedTimeMicro() {
        std::chrono::steady_clock::time_point time_end = std::chrono::steady_clock::now();
        return (std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin).count());
    }

    void reset() {
        time_begin = std::chrono::steady_clock::now();
    }
};



/*
* Author:  David Robert Nadeau
* Site:    http://NadeauSoftware.com/
* License: Creative Commons Attribution 3.0 Unported License
*          http://creativecommons.org/licenses/by/3.0/deed.en_US
*/

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))

#include <unistd.h>
#include <sys/resource.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#include <fcntl.h>
#include <procfs.h>

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

#endif

#else
#error "Cannot define getPeakRSS( ) or getCurrentRSS( ) for an unknown OS."
#endif


/**
* Returns the peak (maximum so far) resident set size (physical
* memory use) measured in bytes, or zero if the value cannot be
* determined on this OS.
*/
static size_t getPeakRSS() {
#if defined(_WIN32)
    /* Windows -------------------------------------------------- */
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.PeakWorkingSetSize;

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
    /* AIX and Solaris ------------------------------------------ */
    struct psinfo psinfo;
    int fd = -1;
    if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
        return (size_t)0L;      /* Can't open? */
    if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo)) {
        close(fd);
        return (size_t)0L;      /* Can't read? */
    }
    close(fd);
    return (size_t)(psinfo.pr_rssize * 1024L);

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
    /* BSD, Linux, and OSX -------------------------------------- */
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
#if defined(__APPLE__) && defined(__MACH__)
    return (size_t)rusage.ru_maxrss;
#else
    return (size_t) (rusage.ru_maxrss * 1024L);
#endif

#else
    /* Unknown OS ----------------------------------------------- */
    return (size_t)0L;          /* Unsupported. */
#endif
}


/**
* Returns the current resident set size (physical memory use) measured
* in bytes, or zero if the value cannot be determined on this OS.
*/
static size_t getCurrentRSS() {
#if defined(_WIN32)
    /* Windows -------------------------------------------------- */
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.WorkingSetSize;

#elif defined(__APPLE__) && defined(__MACH__)
    /* OSX ------------------------------------------------------ */
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
        (task_info_t)&info, &infoCount) != KERN_SUCCESS)
        return (size_t)0L;      /* Can't access? */
    return (size_t)info.resident_size;

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    /* Linux ---------------------------------------------------- */
    long rss = 0L;
    FILE *fp = NULL;
    if ((fp = fopen("/proc/self/statm", "r")) == NULL)
        return (size_t) 0L;      /* Can't open? */
    if (fscanf(fp, "%*s%ld", &rss) != 1) {
        fclose(fp);
        return (size_t) 0L;      /* Can't read? */
    }
    fclose(fp);
    return (size_t) rss * (size_t) sysconf(_SC_PAGESIZE);

#else
    /* AIX, BSD, Solaris, and Unknown OS ------------------------ */
    return (size_t)0L;          /* Unsupported. */
#endif
}


static void
get_gt(
    unsigned int *massQA,
    unsigned char *massQ,
    unsigned char *mass,
    size_t vecsize,
    size_t qsize,
    L2SpaceI &l2space,
    size_t vecdim,
    vector<std::priority_queue<std::pair<int, labeltype>>> &answers,
    size_t k) {
    (vector<std::priority_queue<std::pair<int, labeltype >>>(qsize)).swap(answers);
    DISTFUNC<int> fstdistfunc_ = l2space.get_dist_func();
    cout << qsize << "\n";
    for (int i = 0; i < qsize; i++) {
        for (int j = 0; j < k; j++) {
            answers[i].emplace(0.0f, massQA[1000 * i + j]);
        }
    }
}

static float
test_approx(
    unsigned char *massQ,
    size_t vecsize,
    size_t qsize,
    HierarchicalNSW<int> &appr_alg,
    size_t vecdim,
    vector<std::priority_queue<std::pair<int, labeltype>>> &answers,
    size_t k) {
    size_t correct = 0;
    size_t total = 0;
    // uncomment to test in parallel mode:
    //#pragma omp parallel for
    for (int i = 0; i < qsize; i++) {
        std::priority_queue<std::pair<int, labeltype >> result = appr_alg.searchKnn(massQ + vecdim * i, k);
        std::priority_queue<std::pair<int, labeltype >> gt(answers[i]);
        unordered_set<labeltype> g;
        total += gt.size();

        while (gt.size()) {
            g.insert(gt.top().second);
            gt.pop();
        }

        while (result.size()) {
            if (g.find(result.top().second) != g.end()) {
                correct++;
            } else {
            }
            result.pop();
        }
    }
    return 1.0f * correct / total;
}

// Drive the ANNS search with various values of ef
static void
test_vs_recall(
    unsigned char *massQ,
    size_t vecsize,
    size_t qsize,
    HierarchicalNSW<int> &appr_alg,
    size_t vecdim,
    vector<std::priority_queue<std::pair<int, labeltype>>> &answers,
    size_t k) {
    vector<size_t> efs;  // = { 10,10,10,10,10 };
    for (int i = k; i < 30; i++) {
        efs.push_back(i);
    }
    for (int i = 30; i < 100; i += 10) {
        efs.push_back(i);
    }
    for (int i = 100; i < 500; i += 40) {
        efs.push_back(i);
    }
    for (size_t ef : efs) {
        appr_alg.setEf(ef);
        StopW stopw = StopW();

        float recall = test_approx(massQ, vecsize, qsize, appr_alg, vecdim, answers, k);
        float time_us_per_query = stopw.getElapsedTimeMicro() / qsize;

        cout << ef << "\t" << recall << "\t" << time_us_per_query << " us\n";
        if (recall > 1.0) {
            cout << recall << "\t" << time_us_per_query << " us\n";
            break;
        }
    }
}



/**
 * Benchmark the of running a complete query. 
 * @param massQ The query data.
 * @param vecsize The size of the vector.
 * @param qsize The size of the query.
 * @param appr_alg The approximate nearest neighbor algorithm.
 * @param vecdim The dimension of the vector.
 * @param answers The answers.
 * @param k The number of nearest neighbors.
 * @param ef The ef exploration factor. Should be default 40. 
 */
static float
benchmark_single_run(
    unsigned char *massQ,
    size_t vecsize,
    size_t qsize,
    HierarchicalNSW<int> &appr_alg,
    size_t vecdim,
    vector<std::priority_queue<std::pair<int, labeltype>>> &answers,
    size_t k, 
    int ef) {

    appr_alg.setEf(ef);
    StopW stopw = StopW();

    float recall = test_approx(massQ, vecsize, qsize, appr_alg, vecdim, answers, k);
    float time_us_per_query = stopw.getElapsedTimeMicro() / qsize;
    return time_us_per_query;

    // cout << ef << "\t" << recall << "\t" << time_us_per_query << " us\n";
}

/**
 * Benchmark the average time of running a complete query. 
 */
 static float
 benchmark_runs(
     unsigned char *massQ,
     size_t vecsize,
     size_t qsize,
     HierarchicalNSW<int> &appr_alg,
     size_t vecdim,
     vector<std::priority_queue<std::pair<int, labeltype>>> &answers,
     size_t k, 
     int ef,
     int num_runs) {
 
     float total_time_us = 0;
     for (int i = 0; i < num_runs; i++) {
        total_time_us += benchmark_single_run(massQ, vecsize, qsize, appr_alg, vecdim, answers, k, ef);
     }
     return total_time_us / num_runs;
 }
 


inline bool exists_test(const std::string &name) {
    ifstream f(name.c_str());
    return f.good();
}

static void writeFitnessJsonValue(std::ostream &os, float fitness) {
    if (std::isfinite(fitness)) {
        os << fitness;
        return;
    }
    if (std::isnan(fitness)) {
        os << "\"nan\"";
        return;
    }
    if (std::isinf(fitness)) {
        os << (fitness > 0 ? "\"inf\"" : "\"-inf\"");
        return;
    }
    os << "\"unknown\"";
}

static void writeConfigJsonObject(
    std::ostream &os,
    const hnswlib::ZswapConfig &config,
    const std::string &indent,
    const std::string &inner_indent) {
    os << "{\n";
    os << inner_indent << "\"zpool\": \"" << hnswlib::zpoolTypeToString(config.zpool) << "\",\n";
    os << inner_indent << "\"max_pool_percent\": " << std::stoi(hnswlib::maxPoolPercentToString(config.max_pool_percent)) << ",\n";
    os << inner_indent << "\"compressor\": \"" << hnswlib::compressorTypeToString(config.compressor) << "\",\n";
    os << inner_indent << "\"shrinker_enabled\": "
       << (config.shrinker_enabled == hnswlib::ShrinkerEnabled::YES ? "true" : "false") << ",\n";
    os << inner_indent << "\"enabled\": "
       << (config.enabled == hnswlib::Enabled::YES ? "true" : "false") << "\n";
    os << indent << "}";
}


void sift_test1B(int subset_size_millions, int max_pool_percent, const std::string& output_path) {
    // Ensure output directory exists
    std::string mkdir_cmd = "mkdir -p '" + output_path + "'";
    int result = system(mkdir_cmd.c_str());
    if (result != 0) {
        std::cerr << "Failed to create output directory '" << output_path << "'" << std::endl;
        return;
    }

    if (subset_size_millions != 20 && subset_size_millions != 50) {
        cerr << "subset_size_millions must be 20 or 50\n";
        exit(1);
    }
    // int subset_size_millions = 200; // Very weird, it fails when subset_size_millions is 40
    int efConstruction = 40;
    int M = 16;

    size_t vecsize = subset_size_millions * 1000000;

    size_t qsize = 10000;
    size_t vecdim = 128;
    char path_index[1024];
    char path_gt[1024];
    const char *path_q = "../bigann/bigann_query.bvecs";
    const char *path_data = "../bigann/bigann_base.bvecs";
    snprintf(path_index, sizeof(path_index), "sift1b_%dm_ef_%d_M_%d.bin", subset_size_millions, efConstruction, M);

    snprintf(path_gt, sizeof(path_gt), "../bigann/gnd/idx_%dM.ivecs", subset_size_millions);

    unsigned char *massb = new unsigned char[vecdim];

    cout << "Loading GT:\n";
    ifstream inputGT(path_gt, ios::binary);
    unsigned int *massQA = new unsigned int[qsize * 1000];
    for (int i = 0; i < qsize; i++) {
        int t;
        inputGT.read((char *) &t, 4);
        inputGT.read((char *) (massQA + 1000 * i), t * 4);
        if (t != 1000) {
            cout << "err";
            return;
        }
    }
    inputGT.close();

    cout << "Loading queries:\n";
    unsigned char *massQ = new unsigned char[qsize * vecdim];
    ifstream inputQ(path_q, ios::binary);

    for (int i = 0; i < qsize; i++) {
        int in = 0;
        inputQ.read((char *) &in, 4);
        if (in != 128) {
            cout << "file error";
            exit(1);
        }
        inputQ.read((char *) massb, in);
        for (int j = 0; j < vecdim; j++) {
            massQ[i * vecdim + j] = massb[j];
        }
    }
    inputQ.close();


    unsigned char *mass = new unsigned char[vecdim];
    ifstream input(path_data, ios::binary);
    int in = 0;
    L2SpaceI l2space(vecdim);

    HierarchicalNSW<int> *appr_alg;
    if (exists_test(path_index)) {
        cout << "Loading index from " << path_index << ":\n";
        appr_alg = new HierarchicalNSW<int>(&l2space, path_index, false);
        cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";
    } else {
        cout << "Building index:\n";
        appr_alg = new HierarchicalNSW<int>(&l2space, vecsize, M, efConstruction);

        input.read((char *) &in, 4);
        if (in != 128) {
            cout << "file error";
            exit(1);
        }
        input.read((char *) massb, in);

        for (int j = 0; j < vecdim; j++) {
            mass[j] = massb[j] * (1.0f);
        }

        appr_alg->addPoint((void *) (massb), (size_t) 0);
        int j1 = 0;
        StopW stopw = StopW();
        StopW stopw_full = StopW();
        size_t report_every = 100000;
#pragma omp parallel for
        for (int i = 1; i < vecsize; i++) {
            unsigned char mass[128];
            int j2 = 0;
#pragma omp critical
            {
                input.read((char *) &in, 4);
                if (in != 128) {
                    cout << "file error";
                    exit(1);
                }
                input.read((char *) massb, in);
                for (int j = 0; j < vecdim; j++) {
                    mass[j] = massb[j];
                }
                j1++;
                j2 = j1;
                if (j1 % report_every == 0) {
                    cout << j1 / (0.01 * vecsize) << " %, "
                         << report_every / (1000.0 * 1e-6 * stopw.getElapsedTimeMicro()) << " kips " << " Mem: "
                         << getCurrentRSS() / 1000000 << " Mb \n";
                    stopw.reset();
                }
            }
            appr_alg->addPoint((void *) (mass), (size_t) j2);
        }
        input.close();
        cout << "Build time:" << 1e-6 * stopw_full.getElapsedTimeMicro() << "  seconds\n";
        appr_alg->saveIndex(path_index);
    }

    vector<std::priority_queue<std::pair<int, labeltype >>> answers;
    size_t k = 1;
    cout << "Parsing gt:\n";
    get_gt(massQA, massQ, mass, vecsize, qsize, l2space, vecdim, answers, k);
    cout << "Loaded gt\n";

    bool no_evolution = false;
    if (no_evolution) {
        for (int i = 0; i < 1; i++)
            test_vs_recall(massQ, vecsize, qsize, *appr_alg, vecdim, answers, k);
        cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";
        exit(0);
    }

    // Everything prepared, now begin the evolution algorithm to select ZSWAP parameters.
    hnswlib::EvolutionConfig evo_config;
    evo_config.population_size = 18;
    evo_config.generations = 400;
    evo_config.elite_count = 4;  // 10% of population - preserves best solutions
    evo_config.tournament_size = 4;  // Good selection pressure for pop size 50
    // evo_config.crossover_rate = 0.5f;
    evo_config.gene_swap_probability = 0.3f;

    // config mutation rates (max_pool_percent is fixed, so no mutation)
    evo_config.mutation_rates.zpool = 0.2f;
    evo_config.mutation_rates.max_pool_percent = 0.0f;  // Don't evolve max_pool_percent
    evo_config.mutation_rates.compressor = 0.2f;
    evo_config.mutation_rates.shrinker_enabled = 0.1f;
    evo_config.mutation_rates.enabled = 0.0f;  // Don't evolve enabled, keep it always YES

    // Load default configuration from JSON
    hnswlib::ZswapConfig default_config = loadZswapConfigFromJson("/home/ubuntu/hnswlib/zswap_configs/default.json");

    // Convert command-line max_pool_percent to enum
    hnswlib::MaxPoolPercent fixed_max_pool;
    switch (max_pool_percent) {
        case 10: fixed_max_pool = hnswlib::MaxPoolPercent::_10; break;
        case 20: fixed_max_pool = hnswlib::MaxPoolPercent::_20; break;
        case 30: fixed_max_pool = hnswlib::MaxPoolPercent::_30; break;
        case 40: fixed_max_pool = hnswlib::MaxPoolPercent::_40; break;
        case 50: fixed_max_pool = hnswlib::MaxPoolPercent::_50; break;
        case 60: fixed_max_pool = hnswlib::MaxPoolPercent::_60; break;
        default: fixed_max_pool = hnswlib::MaxPoolPercent::_20; break; // fallback
    }

    // Create initial population with default config but fixed max_pool_percent
    std::vector<hnswlib::ZswapConfig> initial_population;
    initial_population.reserve(evo_config.population_size);

    for (int i = 0; i < evo_config.population_size; ++i) {
        hnswlib::ZswapConfig config = default_config;
        config.max_pool_percent = fixed_max_pool;
        config.enabled = hnswlib::Enabled::YES;  // Always enabled
        initial_population.push_back(config);
    }

    auto fitness_fn = [&](const hnswlib::ZswapConfig &candidate) -> float {
        try {
            hnswlib::applyZswapConfig(candidate);
        } catch (const std::exception &ex) {
            cout << "Failed to apply ZSWAP config: " << ex.what() << "\n";
            return std::numeric_limits<float>::infinity();
        }

        constexpr int kEvaluationRuns = 5;
        const float time_us_per_query = benchmark_runs(
            massQ, vecsize, qsize, *appr_alg, vecdim, answers, k, efConstruction, kEvaluationRuns);

        cout << "Evaluated candidate -> avg runtime " << time_us_per_query << " us/query" << "\n";
        return time_us_per_query;
    };

    hnswlib::ZswapEvolution evolution(evo_config, fitness_fn, initial_population);

    auto on_generation = [&, output_path](
        int generation,
        const hnswlib::ZswapConfig &best_config,
        float best_fitness,
        const std::vector<std::pair<hnswlib::ZswapConfig, float>> &population_snapshot) {
        cout << "Generation " << generation
             << " best runtime: " << best_fitness << " us/query"
             << ", compressor=" << hnswlib::compressorTypeToString(best_config.compressor)
             << ", zpool=" << hnswlib::zpoolTypeToString(best_config.zpool)
             << ", max_pool_percent=" << hnswlib::maxPoolPercentToString(best_config.max_pool_percent)
             << ", shrinker=" << hnswlib::shrinkerEnabledToString(best_config.shrinker_enabled)
             << ", enabled=" << hnswlib::enabledToString(best_config.enabled)
             << "\n";

        std::string filename = output_path + "/best_candidate_gen_" + std::to_string(generation) + ".json";
        std::ofstream output(filename);
        if (!output.is_open()) {
            cerr << "Failed to open " << filename << " for writing\n";
            return;
        }

        output.setf(std::ios::fixed, std::ios::floatfield);
        output << std::setprecision(6);

        output << "{\n";
        output << "  \"generation\": " << generation << ",\n";
        output << "  \"best\": {\n";
        output << "    \"fitness\": ";
        writeFitnessJsonValue(output, best_fitness);
        output << ",\n";
        output << "    \"config\": ";
        writeConfigJsonObject(output, best_config, "    ", "      ");
        output << "\n";
        output << "  },\n";
        output << "  \"population\": [\n";

        for (std::size_t i = 0; i < population_snapshot.size(); ++i) {
            const auto &entry = population_snapshot[i];
            output << "    {\n";
            output << "      \"fitness\": ";
            writeFitnessJsonValue(output, entry.second);
            output << ",\n";
            output << "      \"config\": ";
            writeConfigJsonObject(output, entry.first, "      ", "        ");
            output << "\n";
            output << "    }";
            if (i + 1 < population_snapshot.size()) {
                output << ",";
            }
            output << "\n";
        }

        output << "  ]\n";
        output << "}\n";
    };

    auto evolution_result = evolution.run(on_generation);

    cout << "Evolution finished. Best runtime: " << evolution_result.best_fitness << " us/query" << "\n";
    cout << "Best configuration => compressor=" << hnswlib::compressorTypeToString(evolution_result.best_config.compressor)
         << ", zpool=" << hnswlib::zpoolTypeToString(evolution_result.best_config.zpool)
         << ", max_pool_percent=" << hnswlib::maxPoolPercentToString(evolution_result.best_config.max_pool_percent)
         << ", shrinker=" << hnswlib::shrinkerEnabledToString(evolution_result.best_config.shrinker_enabled)
         << ", enabled=" << hnswlib::enabledToString(evolution_result.best_config.enabled)
         << "\n";

    cout << "Actual memory usage: " << getCurrentRSS() / 1000000 << " Mb \n";
    return;
}
