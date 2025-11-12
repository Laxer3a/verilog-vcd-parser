# Build Instructions for Multithreaded VCD Parser

## Prerequisites

### Required Tools

1. **Flex** (version 2.5.35 or later)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install flex

   # Fedora/RHEL
   sudo dnf install flex

   # macOS
   brew install flex
   ```

2. **Bison** (version 3.0 or later)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install bison

   # Fedora/RHEL
   sudo dnf install bison

   # macOS
   brew install bison
   ```

3. **C++ Compiler** with C++11 support (GCC 4.8+, Clang 3.3+)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install build-essential

   # Fedora/RHEL
   sudo dnf groupinstall "Development Tools"

   # macOS (comes with Xcode)
   xcode-select --install
   ```

4. **Make**
   - Usually comes with build tools packages above

### Verify Installation

```bash
flex --version    # Should be 2.5.35+
bison --version   # Should be 3.0+
g++ --version     # Should support C++11
```

## Building the Library

### Step 1: Clean Previous Build (if any)

```bash
make clean
```

### Step 2: Generate Parser and Lexer

```bash
make parser-srcs
```

This generates:
- `build/VCDParser.cpp` and `build/VCDParser.hpp` (from `src/VCDParser.ypp`)
- `build/VCDScanner.cpp` and `build/VCDScanner.hpp` (from `src/VCDScanner.l`)

**Important**: The generated scanner is now **reentrant** (thread-safe).

### Step 3: Build the Library

```bash
make all
```

This creates:
- `build/libverilog-vcd-parser.a` - Static library
- `build/vcd-parse` - Test application

### Step 4: Verify Build

```bash
ls -lh build/
```

You should see:
- `libverilog-vcd-parser.a` (static library)
- Object files (`.o`)
- Generated headers (`.hpp`)

## Building and Running Tests

### Build Multithreading Tests

```bash
make test-multithread
```

Or manually:

```bash
cd test
make
```

### Run Tests

```bash
cd test
./test_multithread
```

Expected output:
```
======================================
VCD Parser Multithreading Test Suite
======================================

=== Test 1: Basic Concurrency (4 threads) ===
[Thread 0] Parsing test_vcd_0.vcd...
[Thread 1] Parsing test_vcd_1.vcd...
...
Results: 4 successful, 0 failed

=== Test 2: Stress Test (20 threads) ===
...
Results: 20 successful, 0 failed

=== Test 3: Variable File Sizes ===
...
Results: 8 successful, 0 failed

=== Test 4: Sequential Reuse of Parser Instance ===
...
Results: 5 successful, 0 failed

======================================
All tests PASSED!
======================================
```

## Using the Library in Your Project

### Option 1: Link Against Static Library

```bash
g++ -std=c++11 -pthread \
    -I/path/to/verilog-vcd-parser/src \
    -I/path/to/verilog-vcd-parser/build \
    your_app.cpp \
    /path/to/verilog-vcd-parser/build/libverilog-vcd-parser.a \
    -o your_app
```

### Option 2: Add to Your Makefile

```makefile
VCD_PARSER_DIR = /path/to/verilog-vcd-parser
VCD_LIB        = $(VCD_PARSER_DIR)/build/libverilog-vcd-parser.a

CXXFLAGS += -I$(VCD_PARSER_DIR)/src -I$(VCD_PARSER_DIR)/build -std=c++11 -pthread
LDFLAGS  += -pthread

your_app: your_app.cpp $(VCD_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $< $(VCD_LIB) $(LDFLAGS)
```

### Option 3: CMake Integration

```cmake
# In your CMakeLists.txt
set(VCD_PARSER_DIR "/path/to/verilog-vcd-parser")

include_directories(
    ${VCD_PARSER_DIR}/src
    ${VCD_PARSER_DIR}/build
)

add_executable(your_app your_app.cpp)

target_link_libraries(your_app
    ${VCD_PARSER_DIR}/build/libverilog-vcd-parser.a
    pthread
)

target_compile_features(your_app PRIVATE cxx_std_11)
```

## Example Application

Here's a minimal multithreaded example:

```cpp
// example_multithread.cpp
#include "VCDFileParser.hpp"
#include <thread>
#include <vector>
#include <iostream>

void parse_file(const std::string& filename) {
    VCDFileParser parser;  // Each thread gets its own parser
    VCDFile* trace = parser.parse_file(filename);

    if (trace) {
        std::cout << filename << ": "
                  << trace->get_signals()->size() << " signals\n";
        delete trace;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file1.vcd> <file2.vcd> ...\n";
        return 1;
    }

    std::vector<std::thread> threads;

    // Parse all files in parallel
    for (int i = 1; i < argc; ++i) {
        threads.emplace_back(parse_file, argv[i]);
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    return 0;
}
```

Build it:

```bash
g++ -std=c++11 -pthread \
    -I./src -I./build \
    example_multithread.cpp \
    build/libverilog-vcd-parser.a \
    -o example_multithread
```

Run it:

```bash
./example_multithread file1.vcd file2.vcd file3.vcd file4.vcd
```

## Troubleshooting

### Issue: `flex: command not found`

**Solution**: Install Flex (see Prerequisites section)

### Issue: `bison: command not found`

**Solution**: Install Bison (see Prerequisites section)

### Issue: Compilation errors about `yylex_init` or `yyscan_t`

**Problem**: Using old generated scanner that wasn't reentrant

**Solution**: Rebuild from scratch:
```bash
make clean
make parser-srcs
make all
```

### Issue: Linker errors about pthread

**Problem**: Missing `-pthread` flag

**Solution**: Add `-pthread` to both compile and link flags:
```bash
g++ -std=c++11 -pthread ... -pthread
```

### Issue: Parser warnings about deprecated directives

These are warnings from Bison about older directive syntax, but they don't affect functionality:
- `%define parser_class_name` → older syntax (still works)
- `%name-prefix` → older syntax (still works)

These can be ignored, or update to newer syntax if desired.

### Issue: Tests fail with "Cannot open file"

**Problem**: Test is trying to parse non-existent VCD files

**Solution**: The test generates its own VCD files. Make sure:
- You have write permissions in the test directory
- You're running the test from the `test/` directory or using `make test-multithread`

## Performance Tips

1. **Thread Count**: Use approximately the number of CPU cores
   ```cpp
   unsigned int num_threads = std::thread::hardware_concurrency();
   ```

2. **Memory**: Each parser instance allocates memory. Monitor usage with many threads.

3. **I/O**: If parsing from disk, consider:
   - Using SSDs for better concurrent read performance
   - Limiting threads if I/O bandwidth is bottleneck

4. **Profiling**: Use tools to identify bottlenecks:
   ```bash
   # Linux: perf
   perf record -g ./test_multithread
   perf report

   # Valgrind: memory checking
   valgrind --tool=helgrind ./test_multithread
   ```

## Advanced: Debugging

### Enable Parser Debug Output

```cpp
VCDFileParser parser;
parser.trace_parsing = true;   // Enable parser debug output
parser.trace_scanning = true;  // Enable lexer debug output
VCDFile* trace = parser.parse_file("test.vcd");
```

### Thread Sanitizer (TSan)

To verify thread safety:

```bash
# Build with TSan
g++ -std=c++11 -pthread -fsanitize=thread -g \
    -I./src -I./build \
    test/test_multithread.cpp \
    build/libverilog-vcd-parser.a \
    -o test_multithread_tsan

# Run (TSan will report any data races)
./test_multithread_tsan
```

### AddressSanitizer (ASan)

To detect memory errors:

```bash
# Build with ASan
g++ -std=c++11 -pthread -fsanitize=address -g \
    -I./src -I./build \
    test/test_multithread.cpp \
    build/libverilog-vcd-parser.a \
    -o test_multithread_asan

# Run (ASan will report memory errors)
./test_multithread_asan
```

## FAQ

**Q: Do I need to rebuild my application?**
A: Yes, because the lexer/parser have been regenerated with reentrant support.

**Q: Is the old API still supported?**
A: Yes! Single-threaded usage remains unchanged. Just create `VCDFileParser` and call `parse_file()`.

**Q: Can I use multiple threads with one parser instance?**
A: No. Each thread should have its own `VCDFileParser` instance. Sharing requires external synchronization.

**Q: Are the parsed results (VCDFile) thread-safe?**
A: No. `VCDFile` objects are not thread-safe for concurrent modification. Use appropriate synchronization if sharing results.

**Q: What if I don't have flex/bison?**
A: Pre-generated sources might work, but it's recommended to install flex/bison for best results with this reentrant version.

## See Also

- [MULTITHREADING.md](MULTITHREADING.md) - Detailed multithreading documentation
- [README.md](README.md) - Project overview
- [test/test_multithread.cpp](test/test_multithread.cpp) - Complete test examples

## Support

For issues or questions:
1. Check this documentation
2. Review MULTITHREADING.md
3. Look at test examples
4. Open an issue on the project repository
