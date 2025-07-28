/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmTryCompileExecutor.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>

#include "cmBuildOptions.h"
#include "cmDuration.h"
#include "cmGlobalGenerator.h"
#include "cmMakefile.h"
#include "cmMessageType.h"
#include "cmState.h"
#include "cmStateTypes.h"
#include "cmStringAlgorithms.h"
#include "cmSystemTools.h"
#include "cmWorkingDirectory.h"
#include "cmake.h"

cmTryCompileExecutor& cmTryCompileExecutor::Instance()
{
  static cmTryCompileExecutor instance;
  return instance;
}

cmTryCompileExecutor::cmTryCompileExecutor()
  : MaxJobs(std::thread::hardware_concurrency())
  , ParallelEnabled(true)
  , Shutdown(false)
  , ActiveJobs(0)
{
  // Check environment variable for max jobs override
  const char* maxJobsEnv = cmSystemTools::GetEnv("CMAKE_TRY_COMPILE_JOBS");
  if (maxJobsEnv) {
    unsigned long envJobs = 0;
    if (cmStrToULong(maxJobsEnv, &envJobs) && envJobs > 0) {
      this->MaxJobs = envJobs;
    }
  }
  
  // Check if parallel try_compile is disabled
  const char* parallelEnv = cmSystemTools::GetEnv("CMAKE_TRY_COMPILE_PARALLEL");
  if (parallelEnv && cmIsOff(parallelEnv)) {
    this->ParallelEnabled = false;
  }
  
  // Ensure at least one job
  if (this->MaxJobs == 0) {
    this->MaxJobs = 1;
  }
  
  // Start worker threads if parallel is enabled
  if (this->ParallelEnabled && this->MaxJobs > 1) {
    for (unsigned int i = 0; i < this->MaxJobs; ++i) {
      this->Workers.emplace_back(&cmTryCompileExecutor::WorkerThread, this);
    }
  }
}

cmTryCompileExecutor::~cmTryCompileExecutor()
{
  // Signal shutdown
  {
    std::unique_lock<std::mutex> lock(this->QueueMutex);
    this->Shutdown = true;
  }
  this->JobAvailable.notify_all();
  
  // Wait for all workers to finish
  for (auto& worker : this->Workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

std::future<int> cmTryCompileExecutor::SubmitJob(
  std::unique_ptr<cmTryCompileJob> job)
{
  auto future = job->ResultPromise.get_future();
  
  if (!this->ParallelEnabled || this->MaxJobs == 1) {
    // Execute synchronously
    this->ExecuteJob(std::move(job));
  } else {
    // Queue for parallel execution
    {
      std::unique_lock<std::mutex> lock(this->QueueMutex);
      this->JobQueue.push(std::move(job));
      this->ActiveJobs++;
    }
    this->JobAvailable.notify_one();
  }
  
  return future;
}

void cmTryCompileExecutor::WaitForAll()
{
  if (!this->ParallelEnabled || this->MaxJobs == 1) {
    return; // Nothing to wait for in synchronous mode
  }
  
  std::unique_lock<std::mutex> lock(this->CompletionMutex);
  this->AllJobsComplete.wait(lock, [this] { 
    return this->ActiveJobs == 0 && this->JobQueue.empty(); 
  });
}

void cmTryCompileExecutor::SetMaxJobs(unsigned int maxJobs)
{
  this->MaxJobs = std::max(1u, maxJobs);
}

void cmTryCompileExecutor::SetParallelEnabled(bool enabled)
{
  this->ParallelEnabled = enabled;
}

bool cmTryCompileExecutor::IsParallelEnabled() const
{
  return this->ParallelEnabled && this->MaxJobs > 1;
}

void cmTryCompileExecutor::WorkerThread()
{
  while (true) {
    std::unique_ptr<cmTryCompileJob> job;
    
    // Get next job from queue
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
    
    if (job) {
      this->ExecuteJob(std::move(job));
      
      // Notify completion
      {
        std::unique_lock<std::mutex> lock(this->CompletionMutex);
        this->ActiveJobs--;
      }
      this->AllJobsComplete.notify_all();
    }
  }
}

void cmTryCompileExecutor::ExecuteJob(std::unique_ptr<cmTryCompileJob> job)
{
  // Check result cache first
  std::string cacheKey = job->SourceDir + "|" + job->ProjectName + "|" + 
                        job->TargetName;
  
  {
    std::unique_lock<std::mutex> lock(this->CacheMutex);
    auto it = this->ResultCache.find(cacheKey);
    if (it != this->ResultCache.end()) {
      job->ResultPromise.set_value(it->second);
      return;
    }
  }
  
  // Create unique binary directory for this job
  std::string uniqueBinDir = job->BinaryDir;
  if (this->IsParallelEnabled()) {
    // Add thread ID to ensure uniqueness
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    uniqueBinDir = cmStrCat(job->BinaryDir, "_", oss.str());
  }
  
  // Ensure binary directory exists
  if (!cmSystemTools::FileIsDirectory(uniqueBinDir)) {
    cmSystemTools::MakeDirectory(uniqueBinDir);
  }
  
  // Execute the actual try_compile with unique binary directory
  std::string originalBinaryDir = job->BinaryDir;
  job->BinaryDir = uniqueBinDir;
  
  int result = this->ExecuteTryCompile(job.get());
  
  // Restore original binary directory
  job->BinaryDir = originalBinaryDir;
  
  // Clean up the unique binary directory if we created one
  if (this->IsParallelEnabled() && uniqueBinDir != job->BinaryDir) {
    cmSystemTools::RemoveADirectory(uniqueBinDir);
  }
  
  // Cache the result
  {
    std::unique_lock<std::mutex> lock(this->CacheMutex);
    this->ResultCache[cacheKey] = result;
  }
  
  job->ResultPromise.set_value(result);
}

int cmTryCompileExecutor::ExecuteTryCompile(cmTryCompileJob* job)
{
  // This method replicates the logic from cmMakefile::TryCompile
  // but is thread-safe and works with unique binary directories
  
  // Get debug output setting from parent if available
  bool debugOutput = false;
  if (job->ParentMakefile && job->ParentMakefile->GetCMakeInstance()) {
    debugOutput = job->ParentMakefile->GetCMakeInstance()->GetDebugOutput();
  }
  
  if (debugOutput) {
    std::cerr << "[NIX-DEBUG] ExecuteTryCompile STARTED: " << job->ProjectName << " / " << job->TargetName << std::endl;
  }
  
  // Ensure binary directory exists
  if (!cmSystemTools::FileIsDirectory(job->BinaryDir)) {
    cmSystemTools::MakeDirectory(job->BinaryDir);
  }

  // Change to the tests directory and run cmake
  cmWorkingDirectory workdir(job->BinaryDir);
  if (workdir.Failed()) {
    job->Output = "Failed to change to binary directory: " + workdir.GetError();
    return 1;
  }

  // Create a cmake instance for this thread
  cmake cm(cmake::RoleProject, cmState::Project,
           cmState::ProjectKind::TryCompile);
  
  auto gg = cm.CreateGlobalGenerator(job->GeneratorName);
  if (!gg) {
    job->Output = "Global generator '" + job->GeneratorName + "' could not be created.";
    return 1;
  }
  cm.SetGlobalGenerator(std::move(gg));

  // Configure cmake instance
  cm.SetHomeDirectory(job->SourceDir);
  cm.SetHomeOutputDirectory(job->BinaryDir);
  cm.SetGeneratorInstance(job->GeneratorInstance);
  cm.SetGeneratorPlatform(job->GeneratorPlatform);
  cm.SetGeneratorToolset(job->GeneratorToolset);
  
  // Copy debug output setting from parent if available
  if (job->ParentMakefile && job->ParentMakefile->GetCMakeInstance()) {
    cm.SetDebugOutputOn(job->ParentMakefile->GetCMakeInstance()->GetDebugOutput());
  }
  
  // Load cache from parent project's build directory first
  if (job->ParentMakefile) {
    std::string parentBuildDir = job->ParentMakefile->GetCurrentBinaryDirectory();
    std::string parentCacheFile = parentBuildDir + "/CMakeCache.txt";
    if (cmSystemTools::FileExists(parentCacheFile)) {
      cm.LoadCache(parentBuildDir);
    }
  }
  
  // Then load any local cache
  cm.LoadCache();
  
  // Add build type if specified
  if (!cm.GetGlobalGenerator()->IsMultiConfig() && !job->BuildType.empty()) {
    cm.AddCacheEntry("CMAKE_BUILD_TYPE", job->BuildType, "Build configuration",
                     cmStateEnums::STRING);
  }
  
  // Add recursion depth if specified
  if (!job->RecursionDepth.empty()) {
    cm.AddCacheEntry("CMAKE_MAXIMUM_RECURSION_DEPTH", job->RecursionDepth,
                     "Maximum recursion depth", cmStateEnums::STRING);
  }
  
  // Add cmake args if provided
  if (job->CMakeArgs) {
    cm.SetWarnUnusedCli(false);
    cm.SetCacheArgs(*job->CMakeArgs);
  }
  
  // Add developer warnings setting
  cm.AddCacheEntry("CMAKE_SUPPRESS_DEVELOPER_WARNINGS", 
                   job->SuppressDeveloperWarnings ? "TRUE" : "FALSE", "",
                   cmStateEnums::INTERNAL);
  
  // Enable languages from parent generator (critical for compilation)
  if (job->ParentGenerator && job->ParentMakefile) {
    cm.GetGlobalGenerator()->EnableLanguagesFromGenerator(
      job->ParentGenerator, job->ParentMakefile);
    
    // Explicitly copy essential compiler cache variables from parent
    std::vector<std::string> compilerVars = {
      "CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", 
      "CMAKE_C_COMPILER_ID", "CMAKE_CXX_COMPILER_ID",
      "CMAKE_C_COMPILER_VERSION", "CMAKE_CXX_COMPILER_VERSION",
      "CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS",
      "CMAKE_C_FLAGS_DEBUG", "CMAKE_CXX_FLAGS_DEBUG",
      "CMAKE_C_FLAGS_RELEASE", "CMAKE_CXX_FLAGS_RELEASE"
    };
    
    for (const std::string& var : compilerVars) {
      if (cmValue value = job->ParentMakefile->GetDefinition(var)) {
        cm.AddCacheEntry(var, *value, "", cmStateEnums::FILEPATH);
      }
    }
    
    // Set appropriate make program based on generator
    if (job->GeneratorName == "Unix Makefiles") {
      // For Unix Makefiles, use standard make
      cm.AddCacheEntry("CMAKE_MAKE_PROGRAM", "make", "", cmStateEnums::FILEPATH);
    } else if (job->GeneratorName == "Nix") {
      // For Nix generator, use nix-build
      cm.AddCacheEntry("CMAKE_MAKE_PROGRAM", "nix-build", "", cmStateEnums::FILEPATH);
    } else if (cmValue makeProgram = job->ParentMakefile->GetDefinition("CMAKE_MAKE_PROGRAM")) {
      // For other generators, inherit from parent
      cm.AddCacheEntry("CMAKE_MAKE_PROGRAM", *makeProgram, "", cmStateEnums::FILEPATH);
    }
  }

  // Configure
  if (debugOutput) {
    std::cerr << "[NIX-DEBUG] Starting configure..." << std::endl;
  }
  if (cm.Configure() != 0) {
    job->Output = "Failed to configure test project build system.";
    if (debugOutput) {
      std::cerr << "[NIX-DEBUG] Configure failed!" << std::endl;
    }
    return 1;
  }
  if (debugOutput) {
    std::cerr << "[NIX-DEBUG] Configure succeeded" << std::endl;
  }

  // Generate
  if (debugOutput) {
    std::cerr << "[NIX-DEBUG] Starting generate..." << std::endl;
  }
  if (cm.Generate() != 0) {
    job->Output = "Failed to generate test project build system.";
    if (debugOutput) {
      std::cerr << "[NIX-DEBUG] Generate failed!" << std::endl;
    }
    return 1;
  }
  if (debugOutput) {
    std::cerr << "[NIX-DEBUG] Generate succeeded" << std::endl;
  }

  // Build the project
  std::ostringstream buildOutput;
  int ret = cm.GetGlobalGenerator()->Build(
    1, job->SourceDir, job->BinaryDir, job->ProjectName, 
    job->TargetName.empty() ? std::vector<std::string>{} : std::vector<std::string>{job->TargetName},
    buildOutput, "", "", 
    cmBuildOptions(false, job->Fast, PackageResolveMode::Disable),
    true, cmDuration::zero(), cmSystemTools::OUTPUT_NONE);
  
  job->Output = buildOutput.str();
  
  if (debugOutput) {
    std::cerr << "[NIX-DEBUG] ExecuteTryCompile COMPLETED: " << job->ProjectName << " (result=" << ret << ")" << std::endl;
    if (ret != 0) {
      std::cerr << "[NIX-DEBUG] Build output: " << job->Output << std::endl;
    }
  }
  
  return ret;
}

int cmTryCompileExecutor::ExecuteTryCompile(
  const std::string& srcdir, const std::string& bindir,
  const std::string& projectName, const std::string& targetName,
  bool fast, const std::vector<std::string>* cmakeArgs,
  std::string& output)
{
  // Legacy interface - create a temporary job and use the new method
  auto tempJob = std::make_unique<cmTryCompileJob>();
  tempJob->SourceDir = srcdir;
  tempJob->BinaryDir = bindir;
  tempJob->ProjectName = projectName;
  tempJob->TargetName = targetName;
  tempJob->Fast = fast;
  tempJob->CMakeArgs = cmakeArgs;
  tempJob->GeneratorName = "Nix"; // Default
  tempJob->SuppressDeveloperWarnings = true;
  
  int result = this->ExecuteTryCompile(tempJob.get());
  output = tempJob->Output;
  return result;
}