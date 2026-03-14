#include "mpack_codec.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <spectre/nvim.h>

namespace spectre
{

namespace
{
constexpr auto kRequestTimeout = std::chrono::seconds(5);
}

bool NvimRpc::initialize(NvimProcess& process)
{
    process_ = &process;
    read_buf_.resize(256 * 1024);
    read_failed_ = false;
    running_ = true;

    reader_thread_ = std::thread(&NvimRpc::reader_thread_func, this);
    return true;
}

void NvimRpc::shutdown()
{
    running_ = false;
    response_cv_.notify_all();
    if (reader_thread_.joinable())
    {
        reader_thread_.join();
    }
}

RpcResult NvimRpc::request(const std::string& method, const std::vector<MpackValue>& params)
{
    RpcResult rpc_result;
    if (!process_ || !running_)
    {
        return rpc_result;
    }

    uint32_t msgid = next_msgid_++;

    std::vector<char> encoded;
    if (!encode_rpc_request(msgid, method, params, encoded))
    {
        fprintf(stderr, "[rpc] failed to encode request %s\n", method.c_str());
        return rpc_result;
    }

    if (!process_->write(reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size()))
    {
        fprintf(stderr, "[rpc] write failed for request %s\n", method.c_str());
        read_failed_ = true;
        response_cv_.notify_all();
        return rpc_result;
    }

    std::unique_lock<std::mutex> lock(response_mutex_);
    bool ready = response_cv_.wait_for(lock, kRequestTimeout, [&]() {
        return responses_.count(msgid) > 0 || !running_ || read_failed_ || (process_ && !process_->is_running());
    });

    if (!ready || responses_.count(msgid) == 0)
    {
        fprintf(stderr, "[rpc] request timed out or aborted: %s\n", method.c_str());
        responses_.erase(msgid);
        return rpc_result;
    }

    auto resp = std::move(responses_[msgid]);
    responses_.erase(msgid);
    rpc_result.result = std::move(resp.result);
    rpc_result.error = std::move(resp.error);
    rpc_result.transport_ok = true;
    return rpc_result;
}

void NvimRpc::notify(const std::string& method, const std::vector<MpackValue>& params)
{
    if (!process_ || !running_)
    {
        return;
    }

    std::vector<char> encoded;
    if (!encode_rpc_notification(method, params, encoded))
    {
        fprintf(stderr, "[rpc] failed to encode notification %s\n", method.c_str());
        return;
    }

    if (!process_->write(reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size()))
    {
        fprintf(stderr, "[rpc] write failed for notification %s\n", method.c_str());
        read_failed_ = true;
        response_cv_.notify_all();
    }
}

std::vector<RpcNotification> NvimRpc::drain_notifications()
{
    std::lock_guard<std::mutex> lock(notif_mutex_);
    std::vector<RpcNotification> result;
    while (!notifications_.empty())
    {
        result.push_back(std::move(notifications_.front()));
        notifications_.pop();
    }
    return result;
}

void NvimRpc::reader_thread_func()
{
    std::vector<uint8_t> accum;
    accum.reserve(1024 * 1024);

    while (running_)
    {
        int n = process_->read(read_buf_.data(), read_buf_.size());
        if (n <= 0)
        {
            if (!running_)
                break;
            fprintf(stderr, "nvim pipe read error\n");
            read_failed_ = true;
            running_ = false;
            response_cv_.notify_all();
            break;
        }

        accum.insert(accum.end(), read_buf_.begin(), read_buf_.begin() + n);

        while (!accum.empty())
        {
            size_t accum_before = accum.size();
            MpackValue msg;
            size_t consumed = 0;
            if (!decode_mpack_value(accum, msg, &consumed))
                break;

            if (consumed == 0)
                break;

            accum.erase(accum.begin(), accum.begin() + consumed);

            if (msg.type() == MpackValue::Array && msg.as_array().size() >= 3)
            {
                const auto& msg_array = msg.as_array();
                int type = (int)msg_array[0].as_int();

                if (type == 1 && msg_array.size() >= 4)
                {
                    RpcResponse resp;
                    resp.msgid = (uint32_t)msg_array[1].as_int();
                    resp.error = msg_array[2];
                    resp.result = msg_array[3];

                    std::lock_guard<std::mutex> lock(response_mutex_);
                    responses_[resp.msgid] = std::move(resp);
                    response_cv_.notify_all();
                }
                else if (type == 2 && msg_array.size() >= 3)
                {
                    RpcNotification notif;
                    notif.method = msg_array[1].as_str();
                    if (msg_array[2].type() == MpackValue::Array)
                    {
                        notif.params = msg_array[2].as_array();
                    }

                    std::lock_guard<std::mutex> lock(notif_mutex_);
                    notifications_.push(std::move(notif));
                }
            }
        }
    }
}

MpackValue NvimRpc::make_int(int64_t v)
{
    MpackValue val;
    val.storage = v;
    return val;
}
MpackValue NvimRpc::make_uint(uint64_t v)
{
    MpackValue val;
    val.storage = v;
    return val;
}
MpackValue NvimRpc::make_str(const std::string& v)
{
    MpackValue val;
    val.storage = v;
    return val;
}
MpackValue NvimRpc::make_bool(bool v)
{
    MpackValue val;
    val.storage = v;
    return val;
}
MpackValue NvimRpc::make_array(std::vector<MpackValue> v)
{
    MpackValue val;
    val.storage = std::move(v);
    return val;
}
MpackValue NvimRpc::make_map(std::vector<std::pair<MpackValue, MpackValue>> v)
{
    MpackValue val;
    val.storage = std::move(v);
    return val;
}
MpackValue NvimRpc::make_nil()
{
    return MpackValue{};
}

} // namespace spectre
