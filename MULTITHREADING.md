# Multithreading Support in verilog-vcd-parser

## Overview

The verilog-vcd-parser library now supports **full multithreading** and **reentrancy**. You can create multiple parser instances and use them concurrently in separate threads to parse different VCD files simultaneously.

## Key Features

### Thread-Safe Design

- **Reentrant Lexer**: The Flex lexer uses `%option reentrant` to eliminate global state
- **Pure Parser**: The Bison parser (C++ skeleton) is inherently pure with per-instance state
- **Instance-Isolated State**: Each `VCDFileParser` object maintains its own:
  - Scanner state (`yyscan_t`)
  - Parse location tracker
  - Current simulation time
  - Scope stack
  - File handle
  - Input file pointer

### No Global State

All previously global variables have been moved to instance members:
- `static VCDParser::location loc` → `VCDFileParser::loc`
- `VCDTime current_time` → `VCDFileParser::current_time`
- File handles are per-instance

## Usage

### Single-Threaded Parsing (Unchanged API)

The public API remains backward compatible:

```cpp
#include "VCDFileParser.hpp"

VCDFileParser parser;
VCDFile* trace = parser.parse_file("example.vcd");

if (trace) {
    // Process the parsed VCD file
    std::cout << "Signals: " << trace->get_signals()->size() << std::endl;
    delete trace;
}
```

### Multi-Threaded Parsing

Each thread should create its own `VCDFileParser` instance:

```cpp
#include "VCDFileParser.hpp"
#include <thread>
#include <vector>

void parse_vcd_file(const std::string& filename) {
    VCDFileParser parser;  // Each thread gets its own parser instance
    VCDFile* trace = parser.parse_file(filename);

    if (trace) {
        // Process the VCD file
        std::cout << filename << ": "
                  << trace->get_signals()->size()
                  << " signals\n";
        delete trace;
    } else {
        std::cerr << "Failed to parse " << filename << "\n";
    }
}

int main() {
    std::vector<std::string> vcd_files = {
        "test1.vcd",
        "test2.vcd",
        "test3.vcd",
        "test4.vcd"
    };

    std::vector<std::thread> threads;

    // Launch threads to parse files in parallel
    for (const auto& file : vcd_files) {
        threads.emplace_back(parse_vcd_file, file);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    return 0;
}
```

## Implementation Details

### Lexer Changes (VCDScanner.l)

1. Added `%option reentrant` to make scanner reentrant
2. Added `%option bison-bridge bison-locations` for Bison integration
3. Added `%option extra-type="VCDFileParser*"` to store parser context
4. Replaced static `loc` with `driver.loc` (accessed via `yyextra`)
5. Updated all token returns to use `driver.loc`

### Parser Changes (VCDParser.ypp)

1. Moved global `VCDTime current_time` to `VCDFileParser::current_time`
2. Updated all references to use `driver.current_time`
3. Parser is already pure (C++ skeleton is pure by design)

### Driver Changes (VCDFileParser.hpp/cpp)

1. Added instance members:
   - `yyscan_t scanner` - reentrant scanner state
   - `FILE* input_file` - per-instance file handle
   - `VCDTime current_time` - moved from global
   - `VCDParser::location loc` - moved from static

2. Updated `scan_begin()` to:
   - Initialize reentrant scanner with `yylex_init()`
   - Set scanner extra data to `this` pointer
   - Open file and set scanner input

3. Updated `scan_end()` to:
   - Close input file
   - Destroy scanner with `yylex_destroy()`

4. Added `get_next_token()` method to bridge parser and scanner

## Thread Safety Guarantees

### ✅ Safe (Supported)

- **Multiple parser instances in different threads**: Each thread creates its own `VCDFileParser` object
- **Concurrent parsing of different files**: No shared state between instances
- **Memory safety**: Each instance manages its own memory allocations

### ⚠️ Not Thread-Safe (Unsupported)

- **Sharing one parser instance across threads**: Do not call methods on the same `VCDFileParser` object from multiple threads without external synchronization
- **Concurrent access to parsed VCDFile**: The resulting `VCDFile` object is not thread-safe for concurrent modification

## Testing

See `test/test_multithread.cpp` for comprehensive tests including:
- Parsing 100+ files in parallel threads
- Stress testing with various file sizes
- Verification of parse results
- Performance benchmarking

## Build Requirements

- **Flex**: Version 2.5.35 or later (for reentrant scanner support)
- **Bison**: Version 3.0 or later (for C++ parser)
- **C++11** or later (for `std::thread`)

## Performance Considerations

- **CPU-bound**: VCD parsing is CPU-intensive, optimal thread count ≈ number of CPU cores
- **Memory usage**: Each parser instance allocates memory for parse state and results
- **I/O**: If parsing many files from disk, consider I/O bandwidth limitations

## Migration Guide

If you have existing code using this library:

1. **No changes required** for single-threaded code
2. For multi-threaded code:
   - Create one `VCDFileParser` per thread
   - Do not share parser instances across threads
   - Handle parsed `VCDFile` objects appropriately (they are not thread-safe)

## Example: Thread Pool

```cpp
#include "VCDFileParser.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

class VCDThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::string> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop = false;

public:
    VCDThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                VCDFileParser parser;  // One parser per worker thread
                while (true) {
                    std::string filename;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] {
                            return stop || !tasks.empty();
                        });
                        if (stop && tasks.empty()) return;
                        filename = tasks.front();
                        tasks.pop();
                    }

                    VCDFile* trace = parser.parse_file(filename);
                    if (trace) {
                        // Process trace...
                        delete trace;
                    }
                }
            });
        }
    }

    void enqueue(const std::string& filename) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(filename);
        }
        condition.notify_one();
    }

    ~VCDThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& worker : workers) {
            worker.join();
        }
    }
};
```

## FAQ

**Q: Can I share a `VCDFile*` result across threads?**
A: The `VCDFile` object itself is not thread-safe for concurrent modification. If you need to share results, implement your own synchronization or use read-only access.

**Q: How many threads should I use?**
A: Start with the number of CPU cores. VCD parsing is CPU-bound, so more threads than cores may not help.

**Q: Is there any global state remaining?**
A: No. All state is per-instance. The library is fully reentrant.

**Q: Do I need to rebuild my application?**
A: Yes, the lexer and parser need to be regenerated from the updated .l and .ypp files using flex and bison.

## License

Same as the main project.
