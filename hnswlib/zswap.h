#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstdio>

namespace hnswlib {

// >>> CompressorType >>>
enum class CompressorType {
    LZO = 0,
    DEFLATE = 1,
    _842 = 2,
    LZ4 = 3,
    LZ4HC = 4,
    ZSTD = 5,
};

inline std::string compressorTypeToString(CompressorType type) {
    switch (type) {
        case CompressorType::DEFLATE: return "deflate";
        case CompressorType::_842: return "842";
        case CompressorType::LZ4: return "lz4";
        case CompressorType::LZ4HC: return "lz4hc";
        case CompressorType::ZSTD: return "zstd";
        case CompressorType::LZO: return "lzo";  // default to lzo
    }
    throw std::runtime_error("Invalid compressor type: " + std::to_string(static_cast<int>(type)));
}

inline CompressorType stringToCompressorType(const std::string& str) {
    if (str == "deflate") return CompressorType::DEFLATE;
    if (str == "842") return CompressorType::_842;
    if (str == "lz4") return CompressorType::LZ4;
    if (str == "lz4hc") return CompressorType::LZ4HC;
    if (str == "zstd") return CompressorType::ZSTD;
    if (str == "lzo") return CompressorType::LZO;
    throw std::runtime_error("Invalid compressor type: " + str);
}
// <<< CompressorType <<<

// >>> Zpool >>>
enum class ZpoolType {
    ZBUD = 0,
    Z3FOLD,
    ZSMALLOC
};

inline std::string zpoolTypeToString(ZpoolType type) {
    switch (type) {
        case ZpoolType::ZBUD: return "zbud";
        case ZpoolType::Z3FOLD: return "z3fold";
        case ZpoolType::ZSMALLOC: return "zsmalloc";
    }
    throw std::runtime_error("Invalid zpool type: " + std::to_string(static_cast<int>(type)));
}

inline ZpoolType stringToZpoolType(const std::string& str) {
    if (str == "zbud") return ZpoolType::ZBUD;
    if (str == "z3fold") return ZpoolType::Z3FOLD;
    if (str == "zsmalloc") return ZpoolType::ZSMALLOC;
    throw std::runtime_error("Invalid zpool type: " + str);
}
// <<< Zpool <<<

// >>> MaxPoolPercent >>>
enum class MaxPoolPercent {
    _10 = 0,
    _20,
    _30, 
    _40,
    _50,
    _60,
};

inline std::string maxPoolPercentToString(MaxPoolPercent type) {
    switch (type) {
        case MaxPoolPercent::_10: return "10";
        case MaxPoolPercent::_20: return "20";
        case MaxPoolPercent::_30: return "30";
        case MaxPoolPercent::_40: return "40";
        case MaxPoolPercent::_50: return "50";
        case MaxPoolPercent::_60: return "60";
    }
    throw std::runtime_error("Invalid max pool percent: " + std::to_string(static_cast<int>(type)));
}

inline MaxPoolPercent stringToMaxPoolPercent(const std::string& str) {
    if (str == "10") return MaxPoolPercent::_10;
    if (str == "20") return MaxPoolPercent::_20;
    if (str == "30") return MaxPoolPercent::_30;
    if (str == "40") return MaxPoolPercent::_40;
    if (str == "50") return MaxPoolPercent::_50;
    if (str == "60") return MaxPoolPercent::_60;
    throw std::runtime_error("Invalid max pool percent: " + str);
}
// <<< MaxPoolPercent <<<

// >>> ShrinkerEnabled >>>
enum class ShrinkerEnabled {
    YES = 0,
    NO
};

inline std::string shrinkerEnabledToString(ShrinkerEnabled type) {
    switch (type) {
        case ShrinkerEnabled::YES: return "Y";
        case ShrinkerEnabled::NO: return "N";
    }
    throw std::runtime_error("Invalid shrinker enabled: " + std::to_string(static_cast<int>(type)));
}

inline ShrinkerEnabled stringToShrinkerEnabled(const std::string& str) {
    if (str == "Y") return ShrinkerEnabled::YES;
    if (str == "N") return ShrinkerEnabled::NO;
    throw std::runtime_error("Invalid shrinker enabled: " + str);
}
// <<< ShrinkerEnabled <<<

// >>> Enabled >>>
enum class Enabled {
    YES = 0,
    NO
};

inline std::string enabledToString(Enabled type) {
    switch (type) {
        case Enabled::YES: return "Y";
        case Enabled::NO: return "N";
    }
    throw std::runtime_error("Invalid enabled: " + std::to_string(static_cast<int>(type)));
}

inline Enabled stringToEnabled(const std::string& str) {
    if (str == "Y") return Enabled::YES;
    if (str == "N") return Enabled::NO;
    throw std::runtime_error("Invalid enabled: " + str);
}
// <<< Enabled <<<

// A struct holding Zswap configuration parameters
struct ZswapConfig {
    ZpoolType zpool;
    MaxPoolPercent max_pool_percent;
    CompressorType compressor;
    ShrinkerEnabled shrinker_enabled;
    Enabled enabled;
};

// Loads Zswap configuration parameters from a file.
// File should be in the format:
// key=value
// (e.g., max_pool_percent=30)
inline ZswapConfig loadZswapConfigFromFile(const std::string& filename) {
    ZswapConfig config = {ZpoolType::ZBUD, MaxPoolPercent::_20, CompressorType::LZ4, ShrinkerEnabled::YES, Enabled::YES};
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open zswap config file: " + filename);
    std::string line;
    while(std::getline(file, line)) {
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        if (key == "max_pool_percent") config.max_pool_percent = stringToMaxPoolPercent(value);
        else if (key == "zpool") config.zpool = stringToZpoolType(value);
        else if (key == "compressor") config.compressor = stringToCompressorType(value);
        else if (key == "shrinker_enabled") config.shrinker_enabled = stringToShrinkerEnabled(value);
        else if (key == "enabled") config.enabled = stringToEnabled(value);
        // Add other keys if needed
    }
    file.close();
    return config;
}

// Write the configuration to the zswap sysfs interface
// To *apply* the config at runtime (Linux only; requires root privileges)
inline void applyZswapConfig(const ZswapConfig& config) {
    // These are the standard sysfs file locations:
    // /sys/module/zswap/parameters/[enabled, max_pool_percent, compressor, ...]
    // /proc/sys/vm/swappiness

    // Helper lambda
    auto write_sysfs = [](const std::string& path, const std::string& value) {
        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            throw std::runtime_error("Cannot write to " + path + " (need root privileges?)");
        }
        ofs << value;
        ofs.close();
    };

    try {
        write_sysfs("/sys/module/zswap/parameters/max_pool_percent", maxPoolPercentToString(config.max_pool_percent));
        write_sysfs("/sys/module/zswap/parameters/compressor", compressorTypeToString(config.compressor));
        write_sysfs("/sys/module/zswap/parameters/zpool", zpoolTypeToString(config.zpool));
        write_sysfs("/sys/module/zswap/parameters/shrinker_enabled", shrinkerEnabledToString(config.shrinker_enabled));
        write_sysfs("/sys/module/zswap/parameters/enabled", enabledToString(config.enabled));
    } catch (const std::exception& e) {
        // On error, throw further
        throw std::runtime_error(std::string("applyZswapConfig failed: ") + e.what());
    }
}

// Save the current (running system's) zswap config to a file
// (as a plain key=value list)
inline void saveCurrentZswapConfigToFile(const std::string& filename) {
    auto read_sysfs = [](const std::string& path) -> std::string {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return "";
        std::string value; std::getline(ifs, value); ifs.close();
        return value;
    };
    ZswapConfig config;
    config.max_pool_percent = stringToMaxPoolPercent(read_sysfs("/sys/module/zswap/parameters/max_pool_percent"));
    config.compressor = stringToCompressorType(read_sysfs("/sys/module/zswap/parameters/compressor"));
    config.zpool = stringToZpoolType(read_sysfs("/sys/module/zswap/parameters/zpool"));
    config.shrinker_enabled = stringToShrinkerEnabled(read_sysfs("/sys/module/zswap/parameters/shrinker_enabled"));
    config.enabled = stringToEnabled(read_sysfs("/sys/module/zswap/parameters/enabled"));
    // type is not saved, default to CUSTOM
    
    std::ofstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Cannot open file for writing: " + filename);
    file << "max_pool_percent=" << maxPoolPercentToString(config.max_pool_percent) << "\n";
    file << "compressor=" << compressorTypeToString(config.compressor) << "\n";
    file << "zpool=" << zpoolTypeToString(config.zpool) << "\n";
    file << "shrinker_enabled=" << shrinkerEnabledToString(config.shrinker_enabled) << "\n";
    file << "enabled=" << enabledToString(config.enabled) << "\n";
    file.close();
}

} // namespace hnswlib

