#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>

#include <ifcg/ifcg.hpp>
#include <ifcg/graphics/mesh.hpp>
#include <ifcg/common/meshBase.hpp>

#include "statistics.hpp"
#include "util.hpp"
#include "task.hpp"

namespace fs = std::filesystem;

using Param = std::function<NoiseConfig(int)>;
using Stats = std::function<void(Statistics&, const std::string&, const NoiseConfig&)>;

void runBenchSeq(std::string testName, Param paramSetter, Stats statsSetter, uint repetitions, uint numSteps, uint intensity);
void runBenchPar(std::string testName, Param paramSetter, Stats statsSetter, uint repetitions, uint numSteps, uint intensity);
void runEngineSeq(std::string testName, Param paramSetter, Stats statsSetter, uint repetitions, uint numSteps, uint intensity);
void runEnginePar(std::string testName, Param paramSetter, Stats statsSetter, uint repetitions, uint numSteps, uint intensity);

struct TestConfig {
    std::string name;
    Param paramSetter;
    Stats statsSetter;
};

std::string folderPath = "/home/andre/projects/pad/results";

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <bench|engine> <paralelo|sequencial>" << std::endl;
        return 1;
    }

    std::string type = argv[1];
    std::string mode = argv[2];

    const uint repetitions = 20;
    const uint numSteps = 10;
    const uint intensity = 100;

    std::vector<TestConfig> testConfigs = {
        {
            "Escala",
            [](int step) {
                NoiseConfig config = {
                    .width = (step + 1) * 100,
                    .height = (step + 1) * 100,
                    .wave = (step + 1) * 50,
                    .freq = 4.0f,
                    .amp = 1.0f,
                    .exp = 1.0f,
                    .seed = (uint)time(NULL),
                    .octaves = 6
                };
                return config;
            },
            [](Statistics& stats, const std::string& group, const NoiseConfig& config) {
                stats.addEntry(group, "Tamanho", (double)config.width);
            }
        },
        {
            "Octaves",
            [](int step) {
                NoiseConfig config = {
                    .width = 400,
                    .height = 400,
                    .wave = 200,
                    .freq = 4.0f,
                    .amp = 1.0f,
                    .exp = 1.0f,
                    .seed = (uint)time(NULL),
                    .octaves = (uint) step + 1
                };
                return config;
            },
            [](Statistics& stats, const std::string& group, const NoiseConfig& config) {
                stats.addEntry(group, "Octaves", (double)config.octaves);
            }
        }
    };

    for (const auto& config : testConfigs) {
        if (type == "bench") {
            if (mode == "paralelo") {
                runBenchPar(config.name, config.paramSetter, config.statsSetter, repetitions, numSteps, intensity);
            } else {
                runBenchSeq(config.name, config.paramSetter, config.statsSetter, repetitions, numSteps, intensity);
            }
        } else {
            if (mode == "paralelo") {
                runEnginePar(config.name, config.paramSetter, config.statsSetter, repetitions, numSteps, intensity);
            } else {
                runEngineSeq(config.name, config.paramSetter, config.statsSetter, repetitions, numSteps, intensity);
            }
        }
    }
    
    return 0;
}

void runBenchSeq(std::string testName, Param paramSetter, Stats statsSetter, uint repetitions, uint numSteps, uint intensity) {
    Statistics stats(numSteps * repetitions);

    auto path = folderPath + "/" + testName;
    fs::create_directories(path);

    for (int step = 0; step < (int)numSteps; ++step) {
        NoiseConfig config = paramSetter(step);
        for (uint rep = 0; rep < repetitions; ++rep) {
            auto start = std::chrono::steady_clock::now();
            auto noise = generateNoiseMap(config);
            auto [v, i] = createMeshDataFromNoise(noise, config.width, config.height, (float)intensity);
            auto end = std::chrono::steady_clock::now();

            double totalTime = std::chrono::duration<double, std::milli>(end - start).count();
            stats.addEntry("Sequencial", "Tempo Geração", totalTime);
            statsSetter(stats, "Sequencial", config);
        }
    }

    stats.saveToCSV(path + "/sequential.csv");
}


void runBenchPar(std::string testName, Param paramSetter, Stats statsSetter, uint repetitions, uint numSteps, uint intensity) {
    TaskMaster tm(false);
    
    int tarefasPorPasso = repetitions * 2; 
    
    std::mutex mainMtx;
    std::condition_variable mainCv;

    Statistics stats(numSteps * repetitions);

    std::string path = folderPath + "/" + testName;
    fs::create_directories(path);
    
    for (uint step = 0; step < numSteps; ++step) {
        std::vector<NoiseConfig> configs(repetitions);
        std::vector<double> times(repetitions);
        
        int tarefasConcluidas = 0;

        for (uint rep = 0; rep < repetitions; ++rep) {
            tm.addTask([step, rep, tarefasPorPasso, paramSetter, intensity, path, &configs, &times, &mainMtx, &mainCv, &tarefasConcluidas, &tm]() {
                NoiseConfig config = paramSetter(step);
                configs[rep] = config;

                auto start = std::chrono::steady_clock::now();
                auto noise = generateNoiseMap(config);

                tm.addTask([config, noise = std::move(noise), rep, tarefasPorPasso, intensity, start, &times, &mainMtx, &mainCv, &tarefasConcluidas]() mutable {
                    auto [v, i] = createMeshDataFromNoise(noise, config.width, config.height, intensity);
                    auto end = std::chrono::steady_clock::now();
                    times[rep] = (double)std::chrono::duration<double, std::milli>(end - start).count();

                    std::lock_guard<std::mutex> lock(mainMtx);
                    tarefasConcluidas++;
                    if (tarefasConcluidas == tarefasPorPasso) mainCv.notify_one();
                }, Priority::Medium);

                {
                    std::lock_guard<std::mutex> lock(mainMtx);
                    tarefasConcluidas++;
                    if (tarefasConcluidas == tarefasPorPasso) mainCv.notify_one();
                }
            }, Priority::High); 
        }

        {
            std::unique_lock<std::mutex> mainLock(mainMtx);
            mainCv.wait(mainLock, [&]() { return tarefasConcluidas == tarefasPorPasso; });
        }

        for (uint rep = 0; rep < repetitions; ++rep) {
            stats.addEntry(testName, "Tempo Total", times[rep]);
            statsSetter(stats, testName, configs[rep]);
        }
    }

    stats.saveToCSV(path + "/paralelo.csv");
};

void runEngineSeq(std::string testName, Param paramSetter, Stats statsSetter, uint repetitions, uint numSteps, uint intensity) {
    ifcg::Engine::init(800, 800, "PAD - Engine Sequencial");
    ifcg::Engine::setup3D();

    Statistics stats(numSteps * repetitions);

    auto path = folderPath + "/" + testName;
    fs::create_directories(path);

    uint currentStep = 0;
    uint currentRep = 0;
    
    auto lastTime = std::chrono::high_resolution_clock::now();
    double fps = 0.0;

    uint totalTasks = numSteps * repetitions;
    uint completedTasks = 0;

    ifcg::LoopConfig config = {
        .loopBody = [&]() {
            if (currentStep >= numSteps) {
                stats.saveToCSV(path + "/engine_sequencial.csv");
                glfwSetWindowShouldClose(Engine::getWindow().getGLFWwindow(), true);
                return;
            }

            auto currentTime = std::chrono::high_resolution_clock::now();
            double deltaTime = std::chrono::duration<double>(currentTime - lastTime).count();
            lastTime = currentTime;

            if (deltaTime > 0) {
                fps = 1.0 / deltaTime;
            }

            std::cout << "\rFPS: " << (int)fps << " | Tasks: " << completedTasks << "/" << totalTasks << "            " << std::flush;

            
            NoiseConfig noiseConfig = paramSetter(currentStep);
            auto start = std::chrono::steady_clock::now();
            auto noise = generateNoiseMap(noiseConfig);
            auto [vertices, indices] = createMeshDataFromNoise(noise, noiseConfig.width, noiseConfig.height, (float)intensity);
            auto end = std::chrono::steady_clock::now();
            double totalTime = std::chrono::duration<double, std::milli>(end - start).count();
            
            completedTasks++;
            
            stats.addEntry(testName, "Tempo Total", totalTime);
            stats.addEntry(testName, "FPS", fps);
            statsSetter(stats, testName, noiseConfig);

            currentRep++;
            if (currentRep >= repetitions) {
                currentRep = 0;
                currentStep++;
            }
        }
    };

    Engine::loop(config);

    ifcg::Engine::terminate();
}

void runEnginePar(std::string testName, Param paramSetter, Stats statsSetter, uint repetitions, uint numSteps, uint intensity) {
    ifcg::Engine::init(800, 800, "PAD - Engine Paralelo");
    ifcg::Engine::setup3D();

    Statistics stats(numSteps * repetitions);
    TaskMaster tm(false);

    auto path = folderPath + "/" + testName;
    fs::create_directories(path);

    auto lastTime = std::chrono::high_resolution_clock::now();
    double fps = 0.0;

    uint totalTasks = numSteps * repetitions;
    uint completedTasks = 0;
    uint facadeCompletedTasks = 0;
    std::mutex taskMtx;

    tm.addTask([numSteps, repetitions, intensity, paramSetter, statsSetter, testName, path, &stats, &tm, &fps, &completedTasks, &facadeCompletedTasks, &taskMtx]() {
        for (uint step = 0; step < numSteps; ++step) {
            for (uint rep = 0; rep < repetitions; ++rep) {
                tm.addTask([step, rep, intensity, paramSetter, statsSetter, testName, path, &stats, &tm, &fps, &completedTasks, &facadeCompletedTasks, &taskMtx]() {
                    NoiseConfig noiseConfig = paramSetter(step);

                    auto start = std::chrono::steady_clock::now();
                    auto noise = generateNoiseMap(noiseConfig);
                    auto [vertices, indices] = createMeshDataFromNoise(noise, noiseConfig.width, noiseConfig.height, (float)intensity);
                    auto end = std::chrono::steady_clock::now();
                    double totalTime = std::chrono::duration<double, std::milli>(end - start).count();

                    {
                        std::lock_guard<std::mutex> lock(taskMtx);
                        facadeCompletedTasks++;
                    }

                    tm.addTask([testName, totalTime, noise, noiseConfig, step, rep, statsSetter, path, &stats, &fps, &completedTasks, &taskMtx]() {
                        stats.addEntry(testName, "Tempo Total", totalTime);
                        stats.addEntry(testName, "FPS", fps);
                        statsSetter(stats, testName, noiseConfig);

                        {
                            std::lock_guard<std::mutex> lock(taskMtx);
                            completedTasks++;
                        }
                    }, Priority::Low);
                }, Priority::Medium);
            }
        }
    }, Priority::High);

    ifcg::LoopConfig config = {
        .loopBody = [&]() {
            auto currentTime = std::chrono::high_resolution_clock::now();
            double deltaTime = std::chrono::duration<double>(currentTime - lastTime).count();
            lastTime = currentTime;
            if (deltaTime > 0) fps = 1.0 / deltaTime;

            std::cout << "\rFPS: " << (int)fps << " | Tasks: " << facadeCompletedTasks << "/" << totalTasks << "            " << std::flush;
            
            {
                std::lock_guard<std::mutex> lock(taskMtx);
                if (completedTasks == numSteps * repetitions) {
                    stats.saveToCSV(path + "/engine_paralelo.csv");
                    glfwSetWindowShouldClose(Engine::getWindow().getGLFWwindow(), true);
                }
            }
        }
    };
    
    Engine::loop(config);

    ifcg::Engine::terminate();
}