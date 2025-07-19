#include "threaded_lib.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <iostream>

class ThreadPool::ThreadPoolImpl {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<size_t> active_count;
    
public:
    ThreadPoolImpl(size_t num_threads) : stop(false), active_count(0) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        
                        if (stop && tasks.empty()) return;
                        
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    
                    active_count++;
                    task();
                    active_count--;
                }
            });
        }
    }
    
    ~ThreadPoolImpl() {
        stop = true;
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }
    
    void enqueue_task(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::move(task));
        }
        condition.notify_one();
    }
    
    void wait_for_all() {
        while (active_count > 0 || !tasks.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    size_t active_threads() const {
        return active_count;
    }
};

ThreadPool::ThreadPool(size_t num_threads) 
    : impl(std::make_unique<ThreadPoolImpl>(num_threads)) {}

ThreadPool::~ThreadPool() = default;

void ThreadPool::enqueue_task(std::function<void()> task) {
    impl->enqueue_task(std::move(task));
}

void ThreadPool::wait_for_all() {
    impl->wait_for_all();
}

size_t ThreadPool::active_threads() const {
    return impl->active_threads();
}

std::vector<int> ParallelProcessor::parallel_map(const std::vector<int>& input,
                                                std::function<int(int)> func) {
    std::vector<int> result(input.size());
    ThreadPool pool(std::thread::hardware_concurrency());
    
    for (size_t i = 0; i < input.size(); ++i) {
        pool.enqueue_task([&result, &input, func, i]() {
            result[i] = func(input[i]);
        });
    }
    
    pool.wait_for_all();
    return result;
}

long ParallelProcessor::parallel_sum(const std::vector<int>& input) {
    if (input.empty()) return 0;
    
    const size_t num_threads = std::thread::hardware_concurrency();
    const size_t chunk_size = input.size() / num_threads;
    
    std::vector<long> partial_sums(num_threads, 0);
    ThreadPool pool(num_threads);
    
    for (size_t t = 0; t < num_threads; ++t) {
        size_t start = t * chunk_size;
        size_t end = (t == num_threads - 1) ? input.size() : start + chunk_size;
        
        pool.enqueue_task([&input, &partial_sums, t, start, end]() {
            for (size_t i = start; i < end; ++i) {
                partial_sums[t] += input[i];
            }
        });
    }
    
    pool.wait_for_all();
    
    long total = 0;
    for (long sum : partial_sums) {
        total += sum;
    }
    return total;
}

std::vector<int> ParallelProcessor::parallel_filter(const std::vector<int>& input,
                                                   std::function<bool(int)> predicate) {
    std::vector<bool> keep(input.size());
    ThreadPool pool(std::thread::hardware_concurrency());
    
    for (size_t i = 0; i < input.size(); ++i) {
        pool.enqueue_task([&keep, &input, predicate, i]() {
            keep[i] = predicate(input[i]);
        });
    }
    
    pool.wait_for_all();
    
    std::vector<int> result;
    for (size_t i = 0; i < input.size(); ++i) {
        if (keep[i]) {
            result.push_back(input[i]);
        }
    }
    return result;
}