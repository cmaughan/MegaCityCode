#pragma once
#include <string>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/types.h>
#include <signal.h>
#endif

namespace spectre {

class NvimProcess {
public:
    bool spawn(const std::string& nvim_path = "nvim");
    void shutdown();

    // Write data to nvim's stdin
    bool write(const uint8_t* data, size_t len);

    // Read data from nvim's stdout (blocking)
    int read(uint8_t* buffer, size_t max_len);

    bool is_running() const;

private:
#ifdef _WIN32
    HANDLE child_stdin_write_ = INVALID_HANDLE_VALUE;
    HANDLE child_stdout_read_ = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION proc_info_ = {};
    bool started_ = false;
#else
    int child_stdin_write_ = -1;
    int child_stdout_read_ = -1;
    pid_t child_pid_ = -1;
    bool started_ = false;
#endif
};

} // namespace spectre
