/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmGlobalNixGenerator.h"

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

#include "cmBuildOptions.h"
#include "cmDuration.h"
#include "cmGlobalGenerator.h"
#include "cmMakefile.h"
#include "cmStringAlgorithms.h"
#include "cmSystemTools.h"
#include "cmake.h"

namespace {

struct TryCompileJob
{
  int Jobs;
  std::string SrcDir;
  std::string BinDir;
  std::string ProjectName;
  std::string Target;
  bool Fast;
  std::promise<int> ResultPromise;
  std::string Output;
};

class ParallelTryCompileExecutor
{
public:
  static ParallelTryCompileExecutor& Instance()
  {
    static ParallelTryCompileExecutor instance;
    return instance;
  }

  std::future<int> SubmitJob(std::unique_ptr<TryCompileJob> job)
  {
    auto future = job->ResultPromise.get_future();
    
    {
      std::unique_lock<std::mutex> lock(this->QueueMutex);
      this->JobQueue.push(std::move(job));
      this->ActiveJobs++;
    }
    this->JobAvailable.notify_one();
    
    return future;
  }
  
  void WaitForAll()
  {
    std::unique_lock<std::mutex> lock(this->CompletionMutex);
    this->AllJobsComplete.wait(lock, [this] { 
      return this->ActiveJobs == 0 && this->JobQueue.empty(); 
    });
  }
  
  void SetGenerator(cmGlobalNixGenerator* gen) { this->Generator = gen; }

private:
  ParallelTryCompileExecutor()
    : Shutdown(false)
    , ActiveJobs(0)
    , Generator(nullptr)
  {
    // Get number of parallel jobs from environment or use hardware concurrency
    unsigned int numThreads = std::thread::hardware_concurrency();
    const char* jobsEnv = cmSystemTools::GetEnv("CMAKE_NIX_TRY_COMPILE_JOBS");
    if (jobsEnv) {
      unsigned long envJobs = 0;
      if (cmStrToULong(jobsEnv, &envJobs) && envJobs > 0) {
        numThreads = envJobs;
      }
    }
    
    // Start worker threads
    for (unsigned int i = 0; i < numThreads; ++i) {
      this->Workers.emplace_back(&ParallelTryCompileExecutor::WorkerThread, this);
    }
  }
  
  ~ParallelTryCompileExecutor()
  {
    {
      std::unique_lock<std::mutex> lock(this->QueueMutex);
      this->Shutdown = true;
    }
    this->JobAvailable.notify_all();
    
    for (auto& worker : this->Workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }
  
  void WorkerThread()
  {
    while (true) {
      std::unique_ptr<TryCompileJob> job;
      
      {
        std::unique_lock<std::mutex> lock(this->QueueMutex);
        this->JobAvailable.wait(lock, [this] { 
          return !this->JobQueue.empty() || this->Shutdown; 
        });
        
        if (this->Shutdown && this->JobQueue.empty()) {
          break;
        }
        
        if (!this->JobQueue.empty()) {
          job = std::move(this->JobQueue.front());
          this->JobQueue.pop();
        }
      }
      
      if (job && this->Generator) {
        // Execute the job using the base class implementation
        job->Output.clear();
        std::vector<std::string> newTarget = {};
        if (!job->Target.empty()) {
          newTarget = { job->Target };
        }
        
        cmBuildOptions defaultBuildOptions(false, job->Fast, 
                                         PackageResolveMode::Disable);
        
        std::stringstream ostr;
        int ret = this->Generator->cmGlobalGenerator::Build(
          job->Jobs, job->SrcDir, job->BinDir, job->ProjectName, 
          newTarget, ostr, "", "", defaultBuildOptions, true,
          cmDuration::zero(), cmSystemTools::OUTPUT_NONE);
        
        job->Output = ostr.str();
        job->ResultPromise.set_value(ret);
        
        {
          std::unique_lock<std::mutex> lock(this->CompletionMutex);
          this->ActiveJobs--;
        }
        this->AllJobsComplete.notify_all();
      }
    }
  }
  
  std::vector<std::thread> Workers;
  std::queue<std::unique_ptr<TryCompileJob>> JobQueue;
  std::mutex QueueMutex;
  std::condition_variable JobAvailable;
  std::atomic<bool> Shutdown;
  
  std::atomic<unsigned int> ActiveJobs;
  std::condition_variable AllJobsComplete;
  std::mutex CompletionMutex;
  
  cmGlobalNixGenerator* Generator;
};

// Storage for pending try_compile operations
thread_local bool InBatchMode = false;
thread_local std::vector<std::unique_ptr<TryCompileJob>> PendingJobs;
thread_local std::vector<std::future<int>> PendingFutures;
thread_local std::vector<std::string*> PendingOutputs;

} // anonymous namespace

// Placeholder for future parallel TryCompile implementation

void cmGlobalNixGenerator::BeginTryCompileBatch()
{
  InBatchMode = true;
  PendingJobs.clear();
  PendingFutures.clear();
  PendingOutputs.clear();
}

void cmGlobalNixGenerator::EndTryCompileBatch()
{
  InBatchMode = false;
  
  // Wait for all jobs to complete
  ParallelTryCompileExecutor::Instance().WaitForAll();
  
  // Collect results
  for (size_t i = 0; i < PendingFutures.size(); ++i) {
    PendingFutures[i].get(); // Wait for completion
    if (i < PendingOutputs.size() && PendingOutputs[i]) {
      // Output was already set in the job
      // In a real implementation, we'd need to properly retrieve it
    }
  }
  
  PendingJobs.clear();
  PendingFutures.clear();
  PendingOutputs.clear();
}