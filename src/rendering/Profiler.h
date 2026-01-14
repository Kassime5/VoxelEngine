//
// Created by maxim on 13/01/2026.
//

#ifndef GLFWVOXEL_PROFILER_H
#define GLFWVOXEL_PROFILER_H

#include <chrono>
#include <string>
#include <unordered_map>
#include <iostream>
#include <iomanip>

class Profiler {
public:
    static Profiler& getInstance() {
        static Profiler instance;
        return instance;
    }

    void startTimer(const std::string& name) {
        m_timers[name] = std::chrono::high_resolution_clock::now();
    }

    void endTimer(const std::string& name) {
        auto end = std::chrono::high_resolution_clock::now();
        auto start = m_timers[name];
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        m_measurements[name].push_back(duration);

        // Keep only last 100 measurements
        if (m_measurements[name].size() > 100) {
            m_measurements[name].erase(m_measurements[name].begin());
        }
    }

    void printStats() {
        std::cout << "\n=== Performance Stats ===\n";
        for (const auto& pair : m_measurements) {
            if (pair.second.empty()) continue;

            long long sum = 0;
            long long max = 0;
            long long min = LLONG_MAX;

            for (long long val : pair.second) {
                sum += val;
                max = std::max(max, val);
                min = std::min(min, val);
            }

            double avg = static_cast<double>(sum) / pair.second.size();

            std::cout << std::setw(30) << std::left << pair.first << ": "
                      << "avg=" << std::setw(8) << std::fixed << std::setprecision(2) << (avg / 1000.0) << "ms "
                      << "min=" << std::setw(8) << (min / 1000.0) << "ms "
                      << "max=" << std::setw(8) << (max / 1000.0) << "ms\n";
        }
        std::cout << "========================\n\n";
    }

    void reset() {
        m_measurements.clear();
        m_timers.clear();
    }

private:
    Profiler() = default;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> m_timers;
    std::unordered_map<std::string, std::vector<long long>> m_measurements;
};

// Helper macro for automatic scope-based timing
class ScopedTimer {
public:
    ScopedTimer(const std::string& name) : m_name(name) {
        Profiler::getInstance().startTimer(m_name);
    }
    ~ScopedTimer() {
        Profiler::getInstance().endTimer(m_name);
    }
private:
    std::string m_name;
};

#define PROFILE_SCOPE(name) ScopedTimer _timer##__LINE__(name)
#define PROFILE_FUNCTION() ScopedTimer _timer##__LINE__(__FUNCTION__)

#endif //GLFWVOXEL_PROFILER_H
