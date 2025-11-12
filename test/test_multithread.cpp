/*!
@file test_multithread.cpp
@brief Comprehensive multithreading tests for VCD parser

Tests the reentrant parser by parsing multiple VCD files concurrently
in separate threads.
*/

#include "VCDFileParser.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cassert>

// Global counters for test results (atomic for thread safety)
std::atomic<int> successful_parses(0);
std::atomic<int> failed_parses(0);

/*!
 * @brief Generate a simple VCD file for testing
 * @param filename Output filename
 * @param num_signals Number of signals to generate
 * @param num_timestamps Number of time steps
 */
void generate_test_vcd(const std::string& filename, int num_signals, int num_timestamps) {
    std::ofstream out(filename);

    out << "$date\n";
    out << "   Test VCD file for multithreading\n";
    out << "$end\n";

    out << "$version\n";
    out << "   VCD Generator 1.0\n";
    out << "$end\n";

    out << "$timescale 1ns $end\n";

    out << "$scope module testbench $end\n";

    // Generate signal declarations
    for (int i = 0; i < num_signals; ++i) {
        out << "$var wire 1 " << (char)('!' + i) << " sig" << i << " $end\n";
    }

    out << "$upscope $end\n";
    out << "$enddefinitions $end\n";

    out << "$dumpvars\n";
    for (int i = 0; i < num_signals; ++i) {
        out << "0" << (char)('!' + i) << "\n";
    }
    out << "$end\n";

    // Generate time steps with value changes
    for (int t = 0; t < num_timestamps; ++t) {
        out << "#" << (t * 10) << "\n";
        for (int i = 0; i < num_signals; ++i) {
            if ((t + i) % 2 == 0) {
                out << "1" << (char)('!' + i) << "\n";
            } else {
                out << "0" << (char)('!' + i) << "\n";
            }
        }
    }

    out.close();
}

/*!
 * @brief Parse a VCD file in a separate thread
 * @param filename VCD file to parse
 * @param thread_id Thread identifier for logging
 * @param expected_signals Expected number of signals (for verification)
 */
void parse_vcd_thread(const std::string& filename, int thread_id, int expected_signals) {
    try {
        // Each thread creates its own parser instance
        VCDFileParser parser;

        std::cout << "[Thread " << thread_id << "] Parsing " << filename << "...\n";

        auto start = std::chrono::high_resolution_clock::now();
        VCDFile* trace = parser.parse_file(filename);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (trace) {
            int signal_count = trace->get_signals()->size();
            int timestamp_count = trace->get_timestamps()->size();

            std::cout << "[Thread " << thread_id << "] Success! "
                      << signal_count << " signals, "
                      << timestamp_count << " timestamps, "
                      << duration.count() << " ms\n";

            // Verify the results
            if (signal_count == expected_signals) {
                successful_parses++;
            } else {
                std::cerr << "[Thread " << thread_id << "] ERROR: Expected "
                          << expected_signals << " signals, got " << signal_count << "\n";
                failed_parses++;
            }

            delete trace;
        } else {
            std::cerr << "[Thread " << thread_id << "] Failed to parse " << filename << "\n";
            failed_parses++;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Thread " << thread_id << "] Exception: " << e.what() << "\n";
        failed_parses++;
    }
}

/*!
 * @brief Test basic concurrent parsing with a small number of threads
 */
void test_basic_concurrency() {
    std::cout << "\n=== Test 1: Basic Concurrency (4 threads) ===\n";

    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::vector<std::string> filenames;

    // Generate test VCD files
    for (int i = 0; i < num_threads; ++i) {
        std::string filename = "test_vcd_" + std::to_string(i) + ".vcd";
        filenames.push_back(filename);
        generate_test_vcd(filename, 10 + i, 100);
    }

    successful_parses = 0;
    failed_parses = 0;

    // Launch threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(parse_vcd_thread, filenames[i], i, 10 + i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Cleanup
    for (const auto& filename : filenames) {
        std::remove(filename.c_str());
    }

    std::cout << "Results: " << successful_parses << " successful, "
              << failed_parses << " failed\n";

    assert(successful_parses == num_threads);
    assert(failed_parses == 0);
}

/*!
 * @brief Stress test with many concurrent threads
 */
void test_stress_many_threads() {
    std::cout << "\n=== Test 2: Stress Test (20 threads) ===\n";

    const int num_threads = 20;
    std::vector<std::thread> threads;
    std::vector<std::string> filenames;

    // Generate test VCD files
    for (int i = 0; i < num_threads; ++i) {
        std::string filename = "stress_test_" + std::to_string(i) + ".vcd";
        filenames.push_back(filename);
        generate_test_vcd(filename, 5, 50);
    }

    successful_parses = 0;
    failed_parses = 0;

    auto start = std::chrono::high_resolution_clock::now();

    // Launch all threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(parse_vcd_thread, filenames[i], i, 5);
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Cleanup
    for (const auto& filename : filenames) {
        std::remove(filename.c_str());
    }

    std::cout << "Total time: " << duration.count() << " ms\n";
    std::cout << "Results: " << successful_parses << " successful, "
              << failed_parses << " failed\n";

    assert(successful_parses == num_threads);
    assert(failed_parses == 0);
}

/*!
 * @brief Test with varying file sizes
 */
void test_variable_sizes() {
    std::cout << "\n=== Test 3: Variable File Sizes ===\n";

    const int num_threads = 8;
    std::vector<std::thread> threads;
    std::vector<std::string> filenames;
    std::vector<int> signal_counts = {5, 10, 20, 40, 80, 100, 150, 200};

    // Generate test VCD files with different sizes
    for (int i = 0; i < num_threads; ++i) {
        std::string filename = "varsize_test_" + std::to_string(i) + ".vcd";
        filenames.push_back(filename);
        generate_test_vcd(filename, signal_counts[i], 100);
    }

    successful_parses = 0;
    failed_parses = 0;

    // Launch threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(parse_vcd_thread, filenames[i], i, signal_counts[i]);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Cleanup
    for (const auto& filename : filenames) {
        std::remove(filename.c_str());
    }

    std::cout << "Results: " << successful_parses << " successful, "
              << failed_parses << " failed\n";

    assert(successful_parses == num_threads);
    assert(failed_parses == 0);
}

/*!
 * @brief Test sequential reuse of parser instances
 */
void test_sequential_reuse() {
    std::cout << "\n=== Test 4: Sequential Reuse of Parser Instance ===\n";

    VCDFileParser parser;
    const int num_files = 5;
    std::vector<std::string> filenames;

    // Generate test files
    for (int i = 0; i < num_files; ++i) {
        std::string filename = "reuse_test_" + std::to_string(i) + ".vcd";
        filenames.push_back(filename);
        generate_test_vcd(filename, 8, 50);
    }

    int success = 0;
    int failed = 0;

    // Parse all files sequentially with the same parser instance
    for (int i = 0; i < num_files; ++i) {
        VCDFile* trace = parser.parse_file(filenames[i]);
        if (trace) {
            if (trace->get_signals()->size() == 8) {
                success++;
                std::cout << "[Reuse " << i << "] Parsed successfully\n";
            } else {
                failed++;
                std::cerr << "[Reuse " << i << "] Signal count mismatch\n";
            }
            delete trace;
        } else {
            failed++;
            std::cerr << "[Reuse " << i << "] Parse failed\n";
        }
    }

    // Cleanup
    for (const auto& filename : filenames) {
        std::remove(filename.c_str());
    }

    std::cout << "Results: " << success << " successful, " << failed << " failed\n";

    assert(success == num_files);
    assert(failed == 0);
}

int main(int argc, char** argv) {
    std::cout << "======================================\n";
    std::cout << "VCD Parser Multithreading Test Suite\n";
    std::cout << "======================================\n";

    try {
        test_basic_concurrency();
        test_stress_many_threads();
        test_variable_sizes();
        test_sequential_reuse();

        std::cout << "\n======================================\n";
        std::cout << "All tests PASSED!\n";
        std::cout << "======================================\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nFATAL ERROR: " << e.what() << "\n";
        return 1;
    }
}
