#pragma once

#include <functional>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>
#include <array>
#include <type_traits>
#include <iostream>
#include <string>

enum class Priority {
    High = 0,
    Medium = 1,
    Low = 2
};

class TaskMaster {
public:
    bool verbose;

    TaskMaster(bool verbose = false) 
        : verbose(verbose) 
    {        
        unsigned int processingUnits = std::thread::hardware_concurrency();
        if (processingUnits > 1) processingUnits--;

        workerStates.assign(processingUnits, '-');

        for (unsigned int id = 0; id < processingUnits; ++id) {
            workers.emplace_back([this, id](std::stop_token st) {
                while (!st.stop_requested()) {
                    std::function<void(std::stop_token)> task;
                    int priorityLevel = -1;

                    {
                        std::unique_lock<std::mutex> lock(mtx);

                        std::function<bool()> stopCon = [this] {
                            return !taskQueues[0].empty() || 
                                   !taskQueues[1].empty() || 
                                   !taskQueues[2].empty();
                        };

                        if (!cv.wait(lock, st, stopCon)) {
                            return;
                        }

                        for (int q = 0; q < 3; ++q) {
                            if (!taskQueues[q].empty()) {
                                task = std::move(taskQueues[q].front());
                                taskQueues[q].pop();
                                priorityLevel = q;
                                break;
                            }
                        }
                    }
                    
                    if (task) {
                        if (this->verbose) drawWorkers(id, priorityLevel);
                        task(st);
                        if (this->verbose) drawWorkers(id, -1); 
                    }
                }
            });
        }
    }

    ~TaskMaster() {
        cv.notify_all();
        for (auto& worker : workers) {
            worker.request_stop();
        }
        if (this->verbose) std::cout << "";
    }

    template <typename Func>
    void addTask(Func&& task, Priority p = Priority::Medium) {
        std::function<void(std::stop_token)> wrappedTask;

        if constexpr (std::is_invocable_v<Func, std::stop_token>) {
            wrappedTask = std::forward<Func>(task);
        } else {
            wrappedTask = [t = std::forward<Func>(task)](std::stop_token) mutable {
                t();
            };
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            taskQueues[static_cast<size_t>(p)].push(std::move(wrappedTask));
        }
        cv.notify_one();
    }

private:
    std::vector<std::jthread> workers;
    std::array<std::queue<std::function<void(std::stop_token)>>, 3> taskQueues;

    std::mutex mtx;
    std::condition_variable_any cv;

    std::mutex printMtx;
    std::string workerStates;

    void drawWorkers(unsigned int workerId, int priorityLevel) {
        std::lock_guard<std::mutex> lock(printMtx);
        
        if (priorityLevel == 0) workerStates[workerId] = 'H';      // High
        else if (priorityLevel == 1) workerStates[workerId] = 'M'; // Medium
        else if (priorityLevel == 2) workerStates[workerId] = 'L'; // Low
        else workerStates[workerId] = '-';                         // Idle

        std::cout << "\r[ ";
        for (char state : workerStates) {
            std::cout << state << " ";
        }
        std::cout << "]"<< std::flush;
    }
};