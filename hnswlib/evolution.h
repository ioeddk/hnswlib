#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "zswap.h"


namespace hnswlib {

struct MutationRates {
    float zpool = 0.05f;
    float max_pool_percent = 0.05f;
    float compressor = 0.05f;
    float shrinker_enabled = 0.05f;
};

struct EvolutionConfig {
    int population_size = 30;
    int generations = 50;
    int elite_count = 2;
    int tournament_size = 3;
    float crossover_rate = 0.7f;
    float gene_swap_probability = 0.5f;
    MutationRates mutation_rates{};
    std::uint32_t random_seed = 0;  // 0 => seed from std::random_device
};

struct EvolutionResult {
    hnswlib::ZswapConfig best_config{};
    float best_fitness = std::numeric_limits<float>::infinity();  // The lower the runtime, the better the fitness. 
    std::vector<float> best_fitness_per_generation{};
};

// FIXME: What does the two lines below do? 
using FitnessFunction = std::function<float(const hnswlib::ZswapConfig&)>;
using GenerationCallback = std::function<void(
    int generation,
    const hnswlib::ZswapConfig& best_config,
    float best_fitness,
    const std::vector<std::pair<hnswlib::ZswapConfig, float>>& population)>;

class ZswapEvolution {
public:
    ZswapEvolution(const EvolutionConfig& config, FitnessFunction fitness);

    EvolutionResult run(const GenerationCallback& on_generation = nullptr);

private:
    struct Individual {
        hnswlib::ZswapConfig config{};
        float fitness = std::numeric_limits<float>::infinity();  // The lower the runtime, the better the fitness. 
    };

    Individual evaluate(const hnswlib::ZswapConfig& config);
    hnswlib::ZswapConfig randomIndividualConfig();
    Individual randomIndividual();
    hnswlib::ZswapConfig mutate(const hnswlib::ZswapConfig& config);
    std::pair<hnswlib::ZswapConfig, hnswlib::ZswapConfig> crossover(const hnswlib::ZswapConfig& parent1, const hnswlib::ZswapConfig& parent2);
    const Individual& tournamentSelect(const std::vector<Individual>& population);

    EvolutionConfig config_;
    FitnessFunction fitness_;
    std::mt19937 rng_;
};

}  // namespace hnswlib