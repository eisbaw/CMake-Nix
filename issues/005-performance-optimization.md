# Issue #005: Build Performance Optimization

**Priority:** MEDIUM  
**Status:** Not Started  
**Category:** Performance  
**Estimated Effort:** 3-4 days

## Problem Description

The fine-grained derivation approach (one per translation unit) provides excellent caching but may have performance overhead for large projects due to:
- Nix evaluation time for many derivations
- Process startup overhead for each compilation
- Potential inefficiencies in parallel scheduling

## Current Behavior

- Each source file gets its own derivation
- No batching or optimization for build performance
- No metrics or profiling data available

## Expected Behavior

The generator should:
1. Provide options for performance tuning
2. Support batched compilation for small files
3. Optimize Nix expression evaluation
4. Enable performance profiling and metrics

## Implementation Plan

### 1. Add Performance Options
```cpp
class cmGlobalNixGenerator {
  // Performance tuning options
  bool EnableBatchCompilation = false;
  int BatchSize = 10;
  bool EnableParallelEvaluation = true;
  
  void InitializeOptions() {
    // Read from CMake cache
    this->EnableBatchCompilation = 
      this->GetCMakeInstance()->IsOn("CMAKE_NIX_BATCH_COMPILATION");
    
    const char* batchSize = 
      this->GetCMakeInstance()->GetCacheDefinition("CMAKE_NIX_BATCH_SIZE");
    if (batchSize) {
      this->BatchSize = std::stoi(batchSize);
    }
  }
};
```

### 2. Implement Batched Compilation
```cpp
void cmNixTargetGenerator::GenerateBatchedObjectDerivations() {
  std::vector<cmSourceFile const*> sources = this->GetSources();
  
  // Group sources into batches
  for (size_t i = 0; i < sources.size(); i += this->BatchSize) {
    size_t end = std::min(i + this->BatchSize, sources.size());
    std::vector<cmSourceFile const*> batch(sources.begin() + i, sources.begin() + end);
    
    this->WriteBatchedObjectDerivation(batch, i / this->BatchSize);
  }
}

void cmNixTargetGenerator::WriteBatchedObjectDerivation(
  const std::vector<cmSourceFile const*>& batch,
  int batchIndex)
{
  content << "  " << this->GetBatchDerivationName(batchIndex) << " = stdenv.mkDerivation {\n";
  content << "    name = \"batch-" << batchIndex << ".o\";\n";
  content << "    src = ./.;\n";
  content << "    buildInputs = [ gcc ];\n";
  
  content << "    buildPhase = ''\n";
  content << "      mkdir -p $out\n";
  for (auto* source : batch) {
    std::string srcPath = this->GetSourcePath(source);
    std::string objName = this->GetObjectFileName(source);
    content << "      gcc -c " << srcPath << " -o $out/" << objName << "\n";
  }
  content << "    '';\n";
  content << "  };\n\n";
}
```

### 3. Optimize Nix Expression Structure
```cpp
void cmGlobalNixGenerator::WriteOptimizedNixFile() {
  // Use let bindings to reduce redundancy
  content << "let\n";
  content << "  # Common build inputs\n";
  content << "  commonBuildInputs = [ gcc ];\n";
  content << "  \n";
  content << "  # Common flags\n";
  content << "  commonFlags = \"-O2\";\n";
  content << "  \n";
  
  // Generate more efficient expressions
  content << "  # Object file derivations\n";
  content << "  objects = {\n";
  
  // Use attribute sets for better organization
  for (auto& target : targets) {
    content << "    " << target->GetName() << " = {\n";
    // Generate target objects
    content << "    };\n";
  }
  
  content << "  };\n";
  content << "in\n";
}
```

### 4. Add Build Metrics Collection
```cpp
class cmNixBuildMetrics {
  void StartTimer(const std::string& phase);
  void EndTimer(const std::string& phase);
  void RecordDerivationCount(int count);
  void GenerateReport();
};

void cmGlobalNixGenerator::Generate() {
  cmNixBuildMetrics metrics;
  metrics.StartTimer("Total Generation");
  
  // ... generation code ...
  
  metrics.RecordDerivationCount(this->derivationCount);
  metrics.EndTimer("Total Generation");
  
  if (this->GetCMakeInstance()->IsOn("CMAKE_NIX_METRICS")) {
    metrics.GenerateReport();
  }
}
```

### 5. Parallel Evaluation Hints
```cpp
void cmGlobalNixGenerator::WriteParallelHints() {
  // Add Nix evaluation hints for parallelism
  content << "# Evaluation hints for parallel builds\n";
  content << "{\n";
  content << "  # Force parallel evaluation of independent derivations\n";
  content << "  inherit (builtins) deepSeq;\n";
  
  // Group independent targets
  content << "  # Independent targets that can be built in parallel\n";
  content << "  parallelTargets = deepSeq [\n";
  for (auto& target : independentTargets) {
    content << "    " << target->GetName() << "\n";
  }
  content << "  ] parallelTargets;\n";
}
```

## Test Cases

1. **Batch Compilation Performance**
   - Large project with 100+ source files
   - Compare build times with/without batching
   - Verify correct compilation results

2. **Parallel Build Efficiency**
   - Multi-target project
   - Measure CPU utilization
   - Verify parallel execution

3. **Incremental Build Performance**
   - Modify single file in large project
   - Measure rebuild time
   - Verify only affected derivations rebuild

4. **Memory Usage**
   - Large project evaluation
   - Monitor Nix evaluator memory usage
   - Ensure reasonable memory consumption

## Acceptance Criteria

- [ ] Batch compilation option works correctly
- [ ] Build times improve for large projects
- [ ] Incremental builds remain efficient
- [ ] Memory usage is reasonable
- [ ] Performance metrics are available
- [ ] No correctness regressions

## Performance Targets

| Metric | Target |
|--------|--------|
| 100-file project initial build | < 2x Makefiles |
| Single-file incremental build | < 1.5x Makefiles |
| Nix evaluation time | < 1 second |
| Memory usage | < 2x Makefiles |

## Future Optimizations

- Distributed build support via Nix remote builders
- Ccache integration for additional caching
- Precompiled header support
- Unity build support

## Related Files

- `Source/cmGlobalNixGenerator.cxx` - Performance options
- `Source/cmNixTargetGenerator.cxx` - Batched compilation
- New file: `Source/cmNixBuildMetrics.cxx` - Metrics collection