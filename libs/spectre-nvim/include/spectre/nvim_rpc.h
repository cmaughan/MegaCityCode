#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mpack.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <signal.h>
#include <sys/types.h>
#endif

namespace spectre
{

// --- NvimProcess ---

class NvimProcess
{
public:
    bool spawn(const std::string& nvim_path = "nvim", const std::vector<std::string>& extra_args = {}, const std::string& working_dir = {});
    void shutdown();

    bool write(const uint8_t* data, size_t len);
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

// --- RPC types ---

struct MpackValue
{
    enum Type
    {
        Nil,
        Bool,
        Int,
        UInt,
        Float,
        String,
        Array,
        Map,
        Ext
    };

    using ArrayStorage = std::vector<MpackValue>;
    using MapStorage = std::vector<std::pair<MpackValue, MpackValue>>;
    using Storage = std::variant<std::monostate, bool, int64_t, uint64_t, double, std::string, ArrayStorage, MapStorage>;

    Storage storage = std::monostate{};

    Type type() const
    {
        switch (storage.index())
        {
        case 0:
            return Nil;
        case 1:
            return Bool;
        case 2:
            return Int;
        case 3:
            return UInt;
        case 4:
            return Float;
        case 5:
            return String;
        case 6:
            return Array;
        case 7:
            return Map;
        default:
            return Nil;
        }
    }

    bool is_nil() const
    {
        return std::holds_alternative<std::monostate>(storage);
    }

    int64_t as_int() const
    {
        if (auto value = std::get_if<int64_t>(&storage))
            return *value;
        if (auto value = std::get_if<uint64_t>(&storage))
            return (int64_t)*value;
        throw std::bad_variant_access();
    }

    const std::string& as_str() const
    {
        return std::get<std::string>(storage);
    }

    bool as_bool() const
    {
        return std::get<bool>(storage);
    }

    const ArrayStorage& as_array() const
    {
        return std::get<ArrayStorage>(storage);
    }

    const MapStorage& as_map() const
    {
        return std::get<MapStorage>(storage);
    }
};

struct RpcNotification
{
    std::string method;
    std::vector<MpackValue> params;
};

struct RpcResponse
{
    uint32_t msgid;
    MpackValue error;
    MpackValue result;
};

struct RpcResult
{
    MpackValue result;
    MpackValue error;
    bool transport_ok = false;

    bool is_error() const
    {
        return !error.is_nil();
    }

    bool ok() const
    {
        return transport_ok && !is_error();
    }
};

class IRpcChannel
{
public:
    virtual ~IRpcChannel() = default;
    virtual RpcResult request(const std::string& method, const std::vector<MpackValue>& params) = 0;
    virtual void notify(const std::string& method, const std::vector<MpackValue>& params) = 0;
};

// --- NvimRpc ---

class NvimRpc : public IRpcChannel
{
public:
    bool initialize(NvimProcess& process);
    void close();
    void shutdown();

    RpcResult request(const std::string& method, const std::vector<MpackValue>& params) override;
    void notify(const std::string& method, const std::vector<MpackValue>& params) override;

    std::vector<RpcNotification> drain_notifications();

    static MpackValue make_int(int64_t v);
    static MpackValue make_uint(uint64_t v);
    static MpackValue make_str(const std::string& v);
    static MpackValue make_bool(bool v);
    static MpackValue make_array(std::vector<MpackValue> v);
    static MpackValue make_map(std::vector<std::pair<MpackValue, MpackValue>> v);
    static MpackValue make_nil();

    std::function<void()> on_notification_available;

private:
    void reader_thread_func();

    NvimProcess* process_ = nullptr;
    std::thread reader_thread_;
    std::atomic<bool> running_{ false };

    std::mutex write_mutex_;
    std::mutex notif_mutex_;
    std::queue<RpcNotification> notifications_;

    std::mutex response_mutex_;
    std::condition_variable response_cv_;
    std::unordered_map<uint32_t, RpcResponse> responses_;

    std::atomic<uint32_t> next_msgid_{ 1 };
    std::atomic<bool> read_failed_{ false };

    std::vector<uint8_t> read_buf_;
    size_t read_pos_ = 0;
    size_t read_len_ = 0;
};

} // namespace spectre
