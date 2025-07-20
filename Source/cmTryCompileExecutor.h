/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <atomic>
#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

class cmMakefile;
class cmGlobalGenerator;

/**
 * \class cmTryCompileJob
 * \brief Represents a single try_compile job that can be executed in parallel
 */
struct cmTryCompileJob
{
  std::string Id;
  std::string SourceDir;
  std::string BinaryDir;
  std::string ProjectName;
  std::string TargetName;
  bool Fast;
  std::vector<std::string> const* CMakeArgs;
  
  // Context from main cmake instance
  std::string GeneratorName;
  std::string GeneratorInstance;
  std::string GeneratorPlatform;
  std::string GeneratorToolset;
  std::string BuildType;
  std::string RecursionDepth;
  bool SuppressDeveloperWarnings;
  
  // Parent generator and makefile for language setup
  cmGlobalGenerator* ParentGenerator;
  cmMakefile* ParentMakefile;
  
  // Results
  std::promise<int> ResultPromise;
  std::string Output;
};

/**
 * \class cmTryCompileExecutor
 * \brief Manages parallel execution of try_compile operations
 *
 * This class provides a thread pool and job queue for executing multiple
 * try_compile operations in parallel, significantly reducing configuration
 * time for projects with many feature tests.
 */
class cmTryCompileExecutor
{
public:
  static cmTryCompileExecutor& Instance();
  
  // Delete copy and move constructors
  cmTryCompileExecutor(cmTryCompileExecutor const&) = delete;
  cmTryCompileExecutor& operator=(cmTryCompileExecutor const&) = delete;
  
  /**
   * Submit a try_compile job for parallel execution
   * Returns a future that will contain the exit code
   */
  std::future<int> SubmitJob(std::unique_ptr<cmTryCompileJob> job);
  
  /**
   * Wait for all submitted jobs to complete
   */
  void WaitForAll();
  
  /**
   * Set the maximum number of parallel jobs
   * Default is std::thread::hardware_concurrency()
   */
  void SetMaxJobs(unsigned int maxJobs);
  
  /**
   * Enable or disable parallel execution
   * When disabled, jobs execute synchronously
   */
  void SetParallelEnabled(bool enabled);
  
  /**
   * Check if parallel execution is enabled and available
   */
  bool IsParallelEnabled() const;

private:
  cmTryCompileExecutor();
  ~cmTryCompileExecutor();
  
  void WorkerThread();
  void ExecuteJob(std::unique_ptr<cmTryCompileJob> job);
  int ExecuteTryCompile(cmTryCompileJob* job);
  
  // Legacy interface for compatibility
  int ExecuteTryCompile(const std::string& srcdir, const std::string& bindir,
                       const std::string& projectName, const std::string& targetName,
                       bool fast, const std::vector<std::string>* cmakeArgs,
                       std::string& output);
  
  // Configuration
  unsigned int MaxJobs;
  bool ParallelEnabled;
  
  // Thread pool
  std::vector<std::thread> Workers;
  std::queue<std::unique_ptr<cmTryCompileJob>> JobQueue;
  std::mutex QueueMutex;
  std::condition_variable JobAvailable;
  std::atomic<bool> Shutdown;
  
  // Active job tracking
  std::atomic<unsigned int> ActiveJobs;
  std::condition_variable AllJobsComplete;
  std::mutex CompletionMutex;
  
  // Result caching (optional optimization)
  std::map<std::string, int> ResultCache;
  std::mutex CacheMutex;
};