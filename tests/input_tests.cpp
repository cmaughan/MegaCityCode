#include "support/test_support.h"

#include <SDL3/SDL.h>
#include <spectre/nvim.h>

#include <string>
#include <utility>
#include <vector>

using namespace spectre;
using namespace spectre::tests;

namespace
{

class FakeRpcChannel final : public IRpcChannel
{
public:
    struct Call
    {
        std::string method;
        std::vector<MpackValue> params;
    };

    RpcResult request(const std::string& method, const std::vector<MpackValue>& params) override
    {
        requests.push_back({ method, params });
        RpcResult result;
        result.transport_ok = true;
        result.result = NvimRpc::make_nil();
        return result;
    }

    void notify(const std::string& method, const std::vector<MpackValue>& params) override
    {
        notifications.push_back({ method, params });
    }

    std::vector<Call> requests;
    std::vector<Call> notifications;
};

} // namespace

void run_input_tests()
{
    run_test("input translates control chords and suppresses duplicate text", []() {
        FakeRpcChannel rpc;
        NvimInput input;
        input.initialize(&rpc, 10, 20);

        input.on_key({ 0, SDLK_A, SDL_KMOD_CTRL, true });
        expect_eq(static_cast<int>(rpc.notifications.size()), 1, "control chord emits one notification");
        expect_eq(rpc.notifications[0].method, std::string("nvim_input"), "control chord uses nvim_input");
        expect_eq(rpc.notifications[0].params[0].as_str(), std::string("<C-a>"), "control chord is translated");

        input.on_text_input({ "a" });
        expect_eq(static_cast<int>(rpc.notifications.size()), 1, "suppressed text input does not emit");
    });

    run_test("input escapes lt and maps mouse coordinates to grid cells", []() {
        FakeRpcChannel rpc;
        NvimInput input;
        input.initialize(&rpc, 10, 20);

        input.on_text_input({ "<" });
        input.on_mouse_button({ SDL_BUTTON_LEFT, true, static_cast<uint16_t>(SDL_KMOD_SHIFT), 15, 25 });
        input.on_mouse_move({ static_cast<uint16_t>(SDL_KMOD_SHIFT), 25, 45 });
        input.on_mouse_wheel({ 0.0f, 1.0f, static_cast<uint16_t>(SDL_KMOD_CTRL), 15, 25 });

        expect_eq(static_cast<int>(rpc.notifications.size()), 4, "text, button, drag, and wheel each emit a notification");
        expect_eq(rpc.notifications[0].params[0].as_str(), std::string("<lt>"), "lt is escaped");
        expect_eq(rpc.notifications[1].method, std::string("nvim_input_mouse"), "mouse uses nvim_input_mouse");
        expect_eq(rpc.notifications[1].params[0].as_str(), std::string("left"), "mouse button name is encoded");
        expect_eq(rpc.notifications[1].params[1].as_str(), std::string("press"), "mouse action is encoded");
        expect_eq(rpc.notifications[1].params[2].as_str(), std::string("S"), "mouse modifiers are encoded");
        expect_eq(rpc.notifications[1].params[4].as_int(), 1, "mouse row is derived from cell height");
        expect_eq(rpc.notifications[1].params[5].as_int(), 1, "mouse col is derived from cell width");
        expect_eq(rpc.notifications[2].params[0].as_str(), std::string("left"), "drag keeps the pressed button");
        expect_eq(rpc.notifications[2].params[1].as_str(), std::string("drag"), "drag action is encoded");
        expect_eq(rpc.notifications[2].params[2].as_str(), std::string("S"), "drag modifiers are encoded");
        expect_eq(rpc.notifications[3].params[0].as_str(), std::string("wheel"), "wheel input uses the wheel button");
        expect_eq(rpc.notifications[3].params[1].as_str(), std::string("up"), "wheel direction is encoded");
        expect_eq(rpc.notifications[3].params[2].as_str(), std::string("C"), "wheel modifiers are encoded");
    });
}
