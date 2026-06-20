#pragma once

#include <iostream>
#include <filesystem>
#include <iomanip>
#include <fstream>
#include <vector>
#include <map>
#include <mutex>
#include <sstream>

class Statistics {
public:
    Statistics(int max_entries = 1)  
        : max_entries(max_entries) {};

    ~Statistics() = default;

    void addEntry(const std::string& group, const std::string& metric, double value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (data[group][metric].size() < max_entries) {
            data[group][metric].push_back(value);
        }
    }

    void printStatistics() const {
        std::lock_guard<std::mutex> lock(mtx);
        const int columnWidth = 20;

        for (const auto& group : data) {
            std::cout << group.first << ":" << std::endl;

            const auto& metrics = group.second;
            if (metrics.empty()) continue;

            bool first = true;
            for (const auto& m : metrics) {
                if (!first) std::cout << ", ";
                std::cout << std::left << std::setw(columnWidth) << m.first;
                first = false;
            }
            std::cout << std::endl;

            size_t numRows = metrics.begin()->second.size();
            for (size_t i = 0; i < numRows; ++i) {
                first = true;
                for (const auto& m : metrics) {
                    if (!first) std::cout << ", ";
                    std::cout << std::left << std::setw(columnWidth) << m.second[i];
                    first = false;
                }
                std::cout << std::endl;
            }
            std::cout << std::string(columnWidth * metrics.size(), '-') << std::endl;
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        data.clear();
    }

    void saveToCSV(const std::string& fullPath) const {
        std::lock_guard<std::mutex> lock(mtx);
        std::ofstream file(fullPath);
        
        if (!file.is_open()) {
            return;
        }

        if (data.empty()) return;

        file << "Grupo";
        const auto& firstGroupMetrics = data.begin()->second;
        for (const auto& m : firstGroupMetrics) {
            file << "," << m.first;
        }
        file << std::endl;

        auto formatValue = [](double v) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << v;
            std::string s = oss.str();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') s.pop_back();
            return s;
        };

        for (const auto& group : data) {
            const std::string& groupName = group.first;
            const auto& metrics = group.second;
            
            size_t numRows = metrics.begin()->second.size();
            for (size_t i = 0; i < numRows; ++i) {
                file << groupName;
                for (const auto& m : metrics) {
                    file << "," << formatValue(m.second[i]);
                }
                file << std::endl;
            }
        }

        file.close();
    }

    void makeCSV(const std::string& filepath) const {
        std::lock_guard<std::mutex> lock(mtx);
        for (const auto& group : data) {
            std::ofstream file(filepath + "/" + group.first + ".csv");
            
            if (!file.is_open()) {
                return;
            }
            
            const auto& metrics = group.second;
            if (metrics.empty()) continue;
            
            bool first = true;
            for (const auto& m : metrics) {
                if (!first) file << ",";
                file << m.first;
                first = false;
            }

            auto formatValue = [](double v) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6) << v;
                std::string s = oss.str();
                s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                if (s.back() == '.') s.pop_back();
                return s;
            };
            
            file << std::endl;
            size_t numRows = metrics.begin()->second.size();
            for (size_t i = 0; i < numRows; ++i) {
                first = true;
                for (const auto& m : metrics) {
                    if (!first) file << ",";
                    file << formatValue(m.second[i]);
                    first = false;
                }
                file << std::endl;
            }

            file.close();
        }
    }

private:
    std::map<std::string, std::map<std::string, std::vector<double>>> data;
    int max_entries;

    mutable std::mutex mtx;
};