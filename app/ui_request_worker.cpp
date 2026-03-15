#include "ui_request_worker.h"

#include <spectre/log.h>

namespace spectre
{

void UiRequestWorker::start(IRpcChannel* rpc)
{
    stop();
    rpc_ = rpc;
    running_ = true;
    thread_ = std::thread(&UiRequestWorker::thread_main, this);
}

void UiRequestWorker::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        has_resize_request_ = false;
    }
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
    rpc_ = nullptr;
}

void UiRequestWorker::request_resize(int cols, int rows, std::string reason)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_)
            return;
        pending_cols_ = cols;
        pending_rows_ = rows;
        pending_reason_ = std::move(reason);
        has_resize_request_ = true;
    }
    cv_.notify_one();
}

void UiRequestWorker::thread_main()
{
    while (true)
    {
        int cols = 0;
        int rows = 0;
        std::string reason;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&]() { return !running_ || has_resize_request_; });
            if (!running_)
                break;

            cols = pending_cols_;
            rows = pending_rows_;
            reason = pending_reason_;
            has_resize_request_ = false;
        }

        if (!rpc_)
            continue;

        auto resize = rpc_->request("nvim_ui_try_resize", { NvimRpc::make_int(cols), NvimRpc::make_int(rows) });
        if (!resize.ok())
        {
            SPECTRE_LOG_WARN(LogCategory::App, "nvim_ui_try_resize failed during %s", reason.c_str());
        }
    }
}

} // namespace spectre
