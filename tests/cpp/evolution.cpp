#include "../../hnswlib/evolution.h"

#include <algorithm>
#include <array>
#include <random>
#include <stdexcept>
#include <utility>

namespace hnswlib {

namespace {

template <typename T, std::size_t N>
T randomChoice(const std::array<T, N>& values, std::mt19937& rng) {
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);
    return values[dist(rng)];
}

template <typename T, std::size_t N>
T randomChoiceExcluding(const std::array<T, N>& values, T current, std::mt19937& rng) {
    if (N == 1) {
        return current;
    }
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);
    T candidate = current;
    do {
        candidate = values[dist(rng)];
    } while (candidate == current);
    return candidate;
}

template <typename T>
T clamp01(T value) {
    if (value < static_cast<T>(0)) {
        return static_cast<T>(0);
    }
    if (value > static_cast<T>(1)) {
        return static_cast<T>(1);
    }
    return value;
}

const std::array<ZpoolType, 3> kZpoolOptions = {
    ZpoolType::ZBUD,
    ZpoolType::Z3FOLD,
    ZpoolType::ZSMALLOC
};

const std::array<MaxPoolPercent, 6> kMaxPoolPercentOptions = {
    MaxPoolPercent::_10,
    MaxPoolPercent::_20,
    MaxPoolPercent::_30,
    MaxPoolPercent::_40,
    MaxPoolPercent::_50,
    MaxPoolPercent::_60
};

const std::array<CompressorType, 6> kCompressorOptions = {
    CompressorType::LZO,
    CompressorType::DEFLATE,
    CompressorType::_842,
    CompressorType::LZ4,
    CompressorType::LZ4HC,
    CompressorType::ZSTD
};

const std::array<ShrinkerEnabled, 2> kShrinkerOptions = {
    ShrinkerEnabled::YES,
    ShrinkerEnabled::NO
};

}  // namespace

/**
 * @brief Constructor for ZswapEvolution. Validate the config values by enforcing positivity and clamping by max possible value. 
 * @param config The configuration for the evolution.
 * @param fitness The fitness function to use.
 */
ZswapEvolution::ZswapEvolution(const EvolutionConfig& config, FitnessFunction fitness)
    : config_(config),
      fitness_(std::move(fitness)),
      rng_(config.random_seed ? config.random_seed : std::random_device{}()) {
    if (!fitness_) {
        throw std::invalid_argument("ZswapEvolution requires a valid fitness function");
    }
    if (config_.population_size <= 0) {
        throw std::invalid_argument("population_size must be positive");
    }
    if (config_.generations <= 0) {
        throw std::invalid_argument("generations must be positive");
    }
    if (config_.tournament_size <= 0) {
        throw std::invalid_argument("tournament_size must be positive");
    }
    if (config_.elite_count < 0) {
        throw std::invalid_argument("elite_count cannot be negative");
    }

    config_.elite_count = std::min(config_.elite_count, config_.population_size);
    config_.tournament_size = std::min(config_.tournament_size, config_.population_size);
    config_.crossover_rate = clamp01(config_.crossover_rate);
    config_.gene_swap_probability = clamp01(config_.gene_swap_probability);
    config_.mutation_rates.zpool = clamp01(config_.mutation_rates.zpool);
    config_.mutation_rates.max_pool_percent = clamp01(config_.mutation_rates.max_pool_percent);
    config_.mutation_rates.compressor = clamp01(config_.mutation_rates.compressor);
    config_.mutation_rates.shrinker_enabled = clamp01(config_.mutation_rates.shrinker_enabled);
}

ZswapEvolution::Individual ZswapEvolution::evaluate(const ZswapConfig& config) {
    Individual result;
    result.config = config;
    result.fitness = fitness_(config);
    return result;
}

ZswapConfig ZswapEvolution::randomIndividualConfig() {
    ZswapConfig config{};
    config.zpool = randomChoice(kZpoolOptions, rng_);
    config.max_pool_percent = randomChoice(kMaxPoolPercentOptions, rng_);
    config.compressor = randomChoice(kCompressorOptions, rng_);
    config.shrinker_enabled = randomChoice(kShrinkerOptions, rng_);
    return config;
}

ZswapEvolution::Individual ZswapEvolution::randomIndividual() {
    return evaluate(randomIndividualConfig());
}

// FIXME: The rate seems to be non independent. I want a random draw for each parameter. 
ZswapConfig ZswapEvolution::mutate(const ZswapConfig& config) {
    ZswapConfig mutated = config;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    if (dist(rng_) < config_.mutation_rates.zpool) {
        mutated.zpool = randomChoiceExcluding(kZpoolOptions, mutated.zpool, rng_);
    }
    if (dist(rng_) < config_.mutation_rates.max_pool_percent) {
        mutated.max_pool_percent = randomChoiceExcluding(kMaxPoolPercentOptions, mutated.max_pool_percent, rng_);
    }
    if (dist(rng_) < config_.mutation_rates.compressor) {
        mutated.compressor = randomChoiceExcluding(kCompressorOptions, mutated.compressor, rng_);
    }
    if (dist(rng_) < config_.mutation_rates.shrinker_enabled) {
        mutated.shrinker_enabled = randomChoiceExcluding(kShrinkerOptions, mutated.shrinker_enabled, rng_);
    }

    return mutated;
}

// FIXME: Similar issue, I want the probability of crossover each point to be independent. 
std::pair<ZswapConfig, ZswapConfig> ZswapEvolution::crossover(const ZswapConfig& parent1, const ZswapConfig& parent2) {
    ZswapConfig child1 = parent1;
    ZswapConfig child2 = parent2;

    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    if (prob(rng_) >= config_.crossover_rate) {
        return {child1, child2};
    }

    if (prob(rng_) < config_.gene_swap_probability) {
        std::swap(child1.zpool, child2.zpool);
    }
    if (prob(rng_) < config_.gene_swap_probability) {
        std::swap(child1.max_pool_percent, child2.max_pool_percent);
    }
    if (prob(rng_) < config_.gene_swap_probability) {
        std::swap(child1.compressor, child2.compressor);
    }
    if (prob(rng_) < config_.gene_swap_probability) {
        std::swap(child1.shrinker_enabled, child2.shrinker_enabled);
    }

    return {child1, child2};
}

const ZswapEvolution::Individual& ZswapEvolution::tournamentSelect(const std::vector<Individual>& population) {
    std::uniform_int_distribution<std::size_t> index_dist(0, population.size() - 1);
    std::size_t best_index = index_dist(rng_);
    float best_fitness = population[best_index].fitness;

    for (int i = 1; i < config_.tournament_size; ++i) {
        std::size_t candidate = index_dist(rng_);
        if (population[candidate].fitness < best_fitness) {
            best_index = candidate;
            best_fitness = population[candidate].fitness;
        }
    }

    return population[best_index];
}

EvolutionResult ZswapEvolution::run(const GenerationCallback& on_generation) {
    std::vector<Individual> population;
    population.reserve(static_cast<std::size_t>(config_.population_size));
    for (int i = 0; i < config_.population_size; ++i) {
        population.push_back(randomIndividual());
    }

    EvolutionResult result{};

    for (int generation = 0; generation < config_.generations; ++generation) {
        std::sort(population.begin(), population.end(), [](const Individual& a, const Individual& b) {
            return a.fitness < b.fitness;
        });

        const Individual& best = population.front();
        if (generation == 0 || best.fitness < result.best_fitness) {
            result.best_config = best.config;
            result.best_fitness = best.fitness;
        }
        result.best_fitness_per_generation.push_back(best.fitness);

        if (on_generation) {
            std::vector<std::pair<hnswlib::ZswapConfig, float>> population_snapshot;
            population_snapshot.reserve(population.size());
            for (const auto& individual : population) {
                population_snapshot.emplace_back(individual.config, individual.fitness);
            }
            on_generation(generation, best.config, best.fitness, population_snapshot);
        }

        if (generation == config_.generations - 1) {
            break;
        }

        std::vector<Individual> next_population;
        next_population.reserve(population.size());

        int elite_count = config_.elite_count;
        for (int i = 0; i < elite_count; ++i) {
            next_population.push_back(population[static_cast<std::size_t>(i)]);
        }

        while (next_population.size() < population.size()) {
            const Individual& parent1 = tournamentSelect(population);
            const Individual& parent2 = tournamentSelect(population);

            // ablation: without crossover
            // auto [child_config1, child_config2] = crossover(parent1.config, parent2.config);
            auto [child_config1, child_config2] = std::make_pair(parent1.config, parent2.config);

            child_config1 = mutate(child_config1);
            next_population.push_back(evaluate(child_config1));

            if (next_population.size() < population.size()) {
                child_config2 = mutate(child_config2);
                next_population.push_back(evaluate(child_config2));
            }
        }

        population = std::move(next_population);
    }

    return result;
}

}  // namespace
