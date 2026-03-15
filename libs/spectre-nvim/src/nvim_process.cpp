#include <spectre/log.h>
#include <spectre/nvim.h>

#include <sstream>

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace spectre
{

#ifdef _WIN32

namespace
{

std::string quote_windows_arg(const std::string& value)
{
    if (value.find_first_of(" \t\"") == std::string::npos)
        return value;

    std::string quoted = "\"";
    size_t backslashes = 0;
    for (char ch : value)
    {
        if (ch == '\\')
        {
            ++backslashes;
            quoted.push_back(ch);
            continue;
        }
        if (ch == '"')
        {
            quoted.append(backslashes, '\\');
            quoted.push_back('\\');
        }
        backslashes = 0;
        quoted.push_back(ch);
    }
    quoted.push_back('"');
    return quoted;
}

} // namespace

bool NvimProcess::spawn(const std::string& nvim_path, const std::vector<std::string>& extra_args, const std::string& working_dir)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdin_read, stdin_write, stdout_read, stdout_write;

    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0))
    {
        SPECTRE_LOG_ERROR(LogCategory::Nvim, "Failed to create stdin pipe");
        return false;
    }
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0))
    {
        SPECTRE_LOG_ERROR(LogCategory::Nvim, "Failed to create stdout pipe");
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return false;
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    HANDLE nul_handle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE,
        &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = nul_handle;
    si.dwFlags |= STARTF_USESTDHANDLES;

    std::ostringstream command;
    command << quote_windows_arg(nvim_path) << " --embed";
    for (const auto& arg : extra_args)
        command << ' ' << quote_windows_arg(arg);
    std::string cmd = command.str();

    const char* cwd = working_dir.empty() ? nullptr : working_dir.c_str();
    if (!CreateProcessA(
            nullptr,
            cmd.data(),
            nullptr, nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr, cwd,
            &si, &proc_info_))
    {
        SPECTRE_LOG_ERROR(LogCategory::Nvim, "Failed to spawn nvim: error %lu", GetLastError());
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        return false;
    }

    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    if (nul_handle != INVALID_HANDLE_VALUE)
        CloseHandle(nul_handle);

    child_stdin_write_ = stdin_write;
    child_stdout_read_ = stdout_read;
    started_ = true;

    SPECTRE_LOG_INFO(LogCategory::Nvim, "nvim spawned (PID %lu)", proc_info_.dwProcessId);
    return true;
}

void NvimProcess::shutdown()
{
    if (!started_)
        return;

    if (child_stdin_write_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(child_stdin_write_);
        child_stdin_write_ = INVALID_HANDLE_VALUE;
    }
    if (child_stdout_read_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(child_stdout_read_);
        child_stdout_read_ = INVALID_HANDLE_VALUE;
    }

    if (proc_info_.hProcess)
    {
        WaitForSingleObject(proc_info_.hProcess, 2000);
        TerminateProcess(proc_info_.hProcess, 0);
        CloseHandle(proc_info_.hProcess);
        CloseHandle(proc_info_.hThread);
    }

    started_ = false;
}

bool NvimProcess::write(const uint8_t* data, size_t len)
{
    DWORD written;
    return WriteFile(child_stdin_write_, data, (DWORD)len, &written, nullptr) && written == len;
}

int NvimProcess::read(uint8_t* buffer, size_t max_len)
{
    DWORD bytes_read;
    if (!ReadFile(child_stdout_read_, buffer, (DWORD)max_len, &bytes_read, nullptr))
    {
        return -1;
    }
    return (int)bytes_read;
}

bool NvimProcess::is_running() const
{
    if (!started_)
        return false;
    DWORD exit_code;
    GetExitCodeProcess(proc_info_.hProcess, &exit_code);
    return exit_code == STILL_ACTIVE;
}

#else // POSIX (macOS, Linux)

bool NvimProcess::spawn(const std::string& nvim_path, const std::vector<std::string>& extra_args, const std::string& working_dir)
{
    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) != 0)
    {
        SPECTRE_LOG_ERROR(LogCategory::Nvim, "Failed to create stdin pipe: %s", strerror(errno));
        return false;
    }
    if (pipe(stdout_pipe) != 0)
    {
        SPECTRE_LOG_ERROR(LogCategory::Nvim, "Failed to create stdout pipe: %s", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        SPECTRE_LOG_ERROR(LogCategory::Nvim, "Failed to fork: %s", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return false;
    }

    if (pid == 0)
    {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);

        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
        {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        if (!working_dir.empty())
            chdir(working_dir.c_str());

        std::vector<char*> argv;
        argv.reserve(extra_args.size() + 3);
        argv.push_back(const_cast<char*>(nvim_path.c_str()));
        argv.push_back(const_cast<char*>("--embed"));
        for (const auto& arg : extra_args)
            argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);

        execvp(nvim_path.c_str(), argv.data());
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    child_stdin_write_ = stdin_pipe[1];
    child_stdout_read_ = stdout_pipe[0];
    child_pid_ = pid;
    started_ = true;

    SPECTRE_LOG_INFO(LogCategory::Nvim, "nvim spawned (PID %d)", (int)child_pid_);
    return true;
}

void NvimProcess::shutdown()
{
    if (!started_)
        return;

    if (child_stdin_write_ >= 0)
    {
        close(child_stdin_write_);
        child_stdin_write_ = -1;
    }
    if (child_stdout_read_ >= 0)
    {
        close(child_stdout_read_);
        child_stdout_read_ = -1;
    }

    if (child_pid_ > 0)
    {
        int status;
        pid_t result = waitpid(child_pid_, &status, WNOHANG);
        if (result == 0)
        {
            kill(child_pid_, SIGTERM);
            usleep(500000);
            result = waitpid(child_pid_, &status, WNOHANG);
            if (result == 0)
            {
                kill(child_pid_, SIGKILL);
                waitpid(child_pid_, &status, 0);
            }
        }
        child_pid_ = -1;
    }

    started_ = false;
}

bool NvimProcess::write(const uint8_t* data, size_t len)
{
    size_t total_written = 0;
    while (total_written < len)
    {
        ssize_t n = ::write(child_stdin_write_, data + total_written, len - total_written);
        if (n <= 0)
            return false;
        total_written += (size_t)n;
    }
    return true;
}

int NvimProcess::read(uint8_t* buffer, size_t max_len)
{
    ssize_t n = ::read(child_stdout_read_, buffer, max_len);
    if (n < 0)
        return -1;
    return (int)n;
}

bool NvimProcess::is_running() const
{
    if (!started_ || child_pid_ <= 0)
        return false;
    int status;
    pid_t result = waitpid(child_pid_, &status, WNOHANG);
    return result == 0;
}

#endif

} // namespace spectre
