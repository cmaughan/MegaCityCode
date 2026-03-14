#pragma once
#include "nvim/nvim_process.h"
#include <mpack.h>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <unordered_map>

namespace spectre {

// Parsed msgpack value (simplified tree)
struct MpackValue {
    enum Type { Nil, Bool, Int, UInt, Float, String, Array, Map, Ext };
    Type type = Nil;

    bool bool_val = false;
    int64_t int_val = 0;
    uint64_t uint_val = 0;
    double float_val = 0;
    std::string str_val;
    std::vector<MpackValue> array_val;
    std::vector<std::pair<MpackValue, MpackValue>> map_val;

    // Helpers
    int64_t as_int() const { return type == UInt ? (int64_t)uint_val : int_val; }
    const std::string& as_str() const { return str_val; }
    bool as_bool() const { return bool_val; }
    const std::vector<MpackValue>& as_array() const { return array_val; }
};

struct RpcNotification {
    std::string method;
    std::vector<MpackValue> params;
};

struct RpcResponse {
    uint32_t msgid;
    MpackValue error;
    MpackValue result;
};

class NvimRpc {
public:
    bool initialize(NvimProcess& process);
    void shutdown();

    // Send an RPC request (blocking for response)
    MpackValue request(const std::string& method, const std::vector<MpackValue>& params);

    // Send an RPC notification (fire and forget)
    void notify(const std::string& method, const std::vector<MpackValue>& params);

    // Drain pending notifications (call from main thread)
    std::vector<RpcNotification> drain_notifications();

    // Helpers to build MpackValues
    static MpackValue make_int(int64_t v);
    static MpackValue make_uint(uint64_t v);
    static MpackValue make_str(const std::string& v);
    static MpackValue make_bool(bool v);
    static MpackValue make_array(std::vector<MpackValue> v);
    static MpackValue make_map(std::vector<std::pair<MpackValue, MpackValue>> v);
    static MpackValue make_nil();

private:
    void reader_thread_func();
    void write_value(mpack_writer_t* writer, const MpackValue& val);
    MpackValue read_value(mpack_reader_t* reader);

    NvimProcess* process_ = nullptr;
    std::thread reader_thread_;
    std::atomic<bool> running_{false};

    // Notifications queue (reader thread -> main thread)
    std::mutex notif_mutex_;
    std::queue<RpcNotification> notifications_;

    // Response map (for blocking requests)
    std::mutex response_mutex_;
    std::condition_variable response_cv_;
    std::unordered_map<uint32_t, RpcResponse> responses_;

    std::atomic<uint32_t> next_msgid_{1};

    // Read buffer
    std::vector<uint8_t> read_buf_;
    size_t read_pos_ = 0;
    size_t read_len_ = 0;
};

} // namespace spectre
