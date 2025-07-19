#!/usr/bin/env python3
"""
CMake Nix Generator Test Validation Framework

This script validates that all implemented features have proper test coverage
and that all test cases pass with the Nix generator.
"""

import os
import subprocess
import sys
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional

class TestValidator:
    def __init__(self, cmake_root: str):
        self.cmake_root = Path(cmake_root)
        self.test_results = {}
        self.feature_coverage = {}
        
    def discover_tests(self) -> List[Path]:
        """Discover all test directories"""
        test_dirs = []
        for item in self.cmake_root.iterdir():
            if item.is_dir() and item.name.startswith('test_'):
                test_dirs.append(item)
        return sorted(test_dirs)
    
    def analyze_test_coverage(self, test_dirs: List[Path]) -> Dict[str, List[str]]:
        """Analyze what features are covered by existing tests"""
        coverage = {
            'basic_compilation': [],
            'header_dependencies': [],
            'shared_libraries': [],
            'static_libraries': [],
            'executables': [],
            'subdirectories': [],
            'find_package': [],
            'custom_commands': [],
            'external_libraries': [],
            'compiler_detection': [],
            'target_linking': [],
            'configuration_types': [],
            'multi_file_projects': []
        }
        
        for test_dir in test_dirs:
            test_name = test_dir.name
            cmake_file = test_dir / "CMakeLists.txt"
            
            if not cmake_file.exists():
                continue
                
            # Read CMakeLists.txt to analyze features
            try:
                with open(cmake_file, 'r') as f:
                    content = f.read().lower()
                    
                # Analyze features based on CMake content
                if 'add_executable' in content:
                    coverage['executables'].append(test_name)
                if 'add_library' in content and 'shared' in content:
                    coverage['shared_libraries'].append(test_name)
                if 'add_library' in content and ('static' in content or 'add_library(' in content):
                    coverage['static_libraries'].append(test_name)
                if 'add_subdirectory' in content:
                    coverage['subdirectories'].append(test_name)
                if 'find_package' in content:
                    coverage['find_package'].append(test_name)
                if 'add_custom_command' in content:
                    coverage['custom_commands'].append(test_name)
                if 'target_link_libraries' in content:
                    coverage['target_linking'].append(test_name)
                    
                # Count source files for multi-file detection
                source_files = list(test_dir.glob("*.c")) + list(test_dir.glob("*.cpp")) + list(test_dir.glob("*.cxx"))
                header_files = list(test_dir.glob("*.h")) + list(test_dir.glob("*.hpp"))
                
                if len(source_files) > 1:
                    coverage['multi_file_projects'].append(test_name)
                if len(header_files) > 0:
                    coverage['header_dependencies'].append(test_name)
                    
                # Basic compilation (all tests should have this)
                if len(source_files) > 0:
                    coverage['basic_compilation'].append(test_name)
                    
            except Exception as e:
                print(f"Warning: Could not analyze {cmake_file}: {e}")
                
        return coverage
    
    def run_test(self, test_dir: Path) -> Tuple[bool, str, Optional[str]]:
        """Run a single test and return (success, output, error)"""
        try:
            test_name = test_dir.name
            original_cwd = os.getcwd()
            
            # Check if this test has a modular justfile
            test_justfile = test_dir / "justfile"
            if test_justfile.exists():
                # Test has its own justfile - run from parent directory
                os.chdir(self.cmake_root)
                cmd = ["just", f"{test_name}::run"]
            else:
                # Old-style test - try to run directly  
                os.chdir(test_dir)
                
                # Try multiple approaches
                if (test_dir / "shell.nix").exists():
                    cmd = ["nix-shell", "--run", "just dev"]
                elif (test_dir / "default.nix").exists():
                    cmd = ["nix-build"]
                else:
                    # Generate default.nix first
                    cmake_path = self.cmake_root / "bin" / "cmake"
                    if cmake_path.exists():
                        subprocess.run([str(cmake_path), "-G", "Nix", "-DCMAKE_MAKE_PROGRAM=nix-build", "."], 
                                     capture_output=True, timeout=60)
                    
                    if (test_dir / "default.nix").exists():
                        cmd = ["nix-build"]
                    else:
                        return False, "", "Could not generate or find Nix build files"
            
            # Run the test command
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )
            
            success = result.returncode == 0
            output = result.stdout
            error = result.stderr if result.stderr else None
            
            # Return to original directory
            os.chdir(original_cwd)
            
            return success, output, error
            
        except subprocess.TimeoutExpired:
            os.chdir(original_cwd)
            return False, "", "Test timed out after 5 minutes"
        except Exception as e:
            os.chdir(original_cwd)
            return False, "", f"Test execution failed: {str(e)}"
    
    def validate_nix_derivation(self, test_dir: Path) -> Tuple[bool, List[str]]:
        """Validate the generated Nix derivation for correctness"""
        issues = []
        
        # Look for default.nix in test directory (most common) or build subdirectory
        nix_file = test_dir / "default.nix"
        if not nix_file.exists():
            build_dir = test_dir / "build"
            nix_file = build_dir / "default.nix"
        
        if not nix_file.exists():
            issues.append("No default.nix generated")
            return False, issues
            
        try:
            with open(nix_file, 'r') as f:
                content = f.read()
                
            # Check for basic Nix syntax requirements
            if not content.startswith("# Generated by CMake Nix Generator"):
                issues.append("Missing generator header comment")
                
            if "with import <nixpkgs> {};" not in content:
                issues.append("Missing nixpkgs import")
                
            if not content.strip().endswith("}"):
                issues.append("Nix file doesn't end with closing brace")
                
            # Check for derivation structure
            if "stdenv.mkDerivation" not in content:
                issues.append("No stdenv.mkDerivation found")
                
            # Check for proper escaping
            if '"""' in content:
                issues.append("Potential string escaping issue with triple quotes")
                
            # Validate Nix syntax by parsing
            try:
                result = subprocess.run(
                    ["nix-instantiate", "--parse", str(nix_file)],
                    capture_output=True,
                    text=True,
                    timeout=30
                )
                if result.returncode != 0:
                    issues.append(f"Nix syntax error: {result.stderr}")
            except:
                issues.append("Could not validate Nix syntax (nix-instantiate not available)")
                
        except Exception as e:
            issues.append(f"Could not read/analyze Nix file: {e}")
            
        return len(issues) == 0, issues
    
    def run_all_tests(self) -> Dict[str, Dict]:
        """Run all tests and collect results"""
        test_dirs = self.discover_tests()
        results = {}
        
        print(f"Found {len(test_dirs)} test directories")
        print("=" * 60)
        
        for test_dir in test_dirs:
            test_name = test_dir.name
            print(f"\nRunning test: {test_name}")
            print("-" * 40)
            
            # Run the test
            success, output, error = self.run_test(test_dir)
            
            # Validate the Nix derivation
            nix_valid, nix_issues = self.validate_nix_derivation(test_dir)
            
            results[test_name] = {
                'success': success,
                'output': output,
                'error': error,
                'nix_valid': nix_valid,
                'nix_issues': nix_issues,
                'path': str(test_dir)
            }
            
            # Print immediate results
            status = "✅ PASS" if success and nix_valid else "❌ FAIL"
            print(f"Status: {status}")
            
            if not success:
                print(f"Execution failed: {error}")
            
            if not nix_valid:
                print(f"Nix validation issues: {', '.join(nix_issues)}")
                
        return results
    
    def generate_report(self, results: Dict[str, Dict]) -> str:
        """Generate a comprehensive test report"""
        test_dirs = self.discover_tests()
        coverage = self.analyze_test_coverage(test_dirs)
        
        total_tests = len(results)
        passed_tests = sum(1 for r in results.values() if r['success'] and r['nix_valid'])
        failed_tests = total_tests - passed_tests
        
        report = []
        report.append("CMake Nix Generator Test Report")
        report.append("=" * 50)
        report.append(f"Total Tests: {total_tests}")
        report.append(f"Passed: {passed_tests}")
        report.append(f"Failed: {failed_tests}")
        report.append(f"Success Rate: {(passed_tests/total_tests*100):.1f}%")
        report.append("")
        
        # Feature Coverage Analysis
        report.append("Feature Coverage Analysis")
        report.append("-" * 30)
        for feature, tests in coverage.items():
            if tests:
                report.append(f"✅ {feature}: {len(tests)} tests ({', '.join(tests)})")
            else:
                report.append(f"❌ {feature}: No tests found")
        report.append("")
        
        # Failed Tests Details
        if failed_tests > 0:
            report.append("Failed Tests Details")
            report.append("-" * 25)
            for test_name, result in results.items():
                if not (result['success'] and result['nix_valid']):
                    report.append(f"\n❌ {test_name}:")
                    if not result['success']:
                        report.append(f"   Execution Error: {result['error']}")
                    if not result['nix_valid']:
                        report.append(f"   Nix Issues: {', '.join(result['nix_issues'])}")
        
        # Recommendations
        report.append("\nRecommendations")
        report.append("-" * 15)
        
        missing_features = [feature for feature, tests in coverage.items() if not tests]
        if missing_features:
            report.append("Missing Test Coverage:")
            for feature in missing_features:
                report.append(f"  - Add tests for {feature}")
        
        if failed_tests > 0:
            report.append("\nImmediate Actions Required:")
            report.append("  - Fix failing tests before proceeding")
            report.append("  - Validate Nix derivation generation")
        
        return "\n".join(report)
    
    def identify_test_gaps(self) -> List[str]:
        """Identify missing test scenarios that should be added"""
        test_dirs = self.discover_tests()
        coverage = self.analyze_test_coverage(test_dirs)
        
        gaps = []
        
        # Critical missing tests
        critical_features = [
            'compiler_detection',
            'configuration_types',
            'external_libraries'
        ]
        
        for feature in critical_features:
            if not coverage[feature]:
                gaps.append(f"Missing {feature} tests")
        
        # Edge cases that should be tested
        edge_cases = [
            "Large projects with many files",
            "Projects with complex header dependencies", 
            "Mixed C/C++ projects",
            "Projects with unusual file extensions",
            "Error handling and malformed CMakeLists.txt",
            "Cross-compilation scenarios",
            "Projects with multiple configurations",
            "Performance tests with large codebases"
        ]
        
        # Check if we have specific edge case tests
        test_names = [d.name for d in test_dirs]
        
        if not any("large" in name or "many" in name for name in test_names):
            gaps.append("Large project tests")
            
        if not any("error" in name or "invalid" in name for name in test_names):
            gaps.append("Error handling tests")
            
        if not any("mixed" in name for name in test_names):
            gaps.append("Mixed language tests")
            
        if not any("performance" in name for name in test_names):
            gaps.append("Performance tests")
        
        return gaps

def main():
    if len(sys.argv) > 1:
        cmake_root = sys.argv[1]
    else:
        cmake_root = os.getcwd()
    
    validator = TestValidator(cmake_root)
    
    print("CMake Nix Generator Test Validation")
    print("=" * 50)
    
    # Run all tests
    results = validator.run_all_tests()
    
    # Generate and display report
    report = validator.generate_report(results)
    print("\n\n" + report)
    
    # Identify test gaps
    gaps = validator.identify_test_gaps()
    if gaps:
        print("\n\nTest Coverage Gaps")
        print("-" * 20)
        for gap in gaps:
            print(f"- {gap}")
    
    # Save detailed results to JSON
    with open('test_results.json', 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"\nDetailed results saved to test_results.json")
    
    # Exit with appropriate code
    total_tests = len(results)
    passed_tests = sum(1 for r in results.values() if r['success'] and r['nix_valid'])
    
    if passed_tests == total_tests:
        print("✅ All tests passed!")
        sys.exit(0)
    else:
        print(f"❌ {total_tests - passed_tests} tests failed")
        sys.exit(1)

if __name__ == "__main__":
    main()