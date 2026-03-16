#pragma once

#include <condition_variable>
#include <mutex>
#include <spectre/nvim_rpc.h>
#include <string>
#include <thread>

namespace spectre
{

class UiRequestWorker
{
public:
    void start(IRpcChannel* rpc);
    void stop();
    void request_resize(int cols, int rows, std::string reason);

private:
    void thread_main();

    IRpcChannel* rpc_ = nullptr;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = false;
    bool has_resize_request_ = false;
    int pending_cols_ = 0;
    int pending_rows_ = 0;
    std::string pending_reason_;
};

} // namespace spectre
