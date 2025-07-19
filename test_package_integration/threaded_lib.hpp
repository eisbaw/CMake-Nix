#ifndef THREADED_LIB_HPP
#define THREADED_LIB_HPP

#include <vector>
#include <string>
#include <functional>
#include <memory>

class ThreadPool {
private:
    class ThreadPoolImpl;
    std::unique_ptr<ThreadPoolImpl> impl;
    
public:
    ThreadPool(size_t num_threads = 4);
    ~ThreadPool();
    
    void enqueue_task(std::function<void()> task);
    void wait_for_all();
    size_t active_threads() const;
};

class ParallelProcessor {
public:
    static std::vector<int> parallel_map(const std::vector<int>& input, 
                                        std::function<int(int)> func);
    static long parallel_sum(const std::vector<int>& input);
    static std::vector<int> parallel_filter(const std::vector<int>& input,
                                           std::function<bool(int)> predicate);
};

#endif // THREADED_LIB_HPP