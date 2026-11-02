// SPDX-License-Identifier: MIT
#include <cstdio>
#include <cstdlib>
#include "application.h"
#include "board.h"
#include "mcp_server.h"
#define CHECK(x)                                                 \
    do {                                                         \
        if (!(x)) {                                              \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); \
            std::abort();                                        \
        }                                                        \
    } while (0)
void RegisterGeneratedAudioTools(McpServer& server);
int main() {
    McpServer server;
    RegisterGeneratedAudioTools(server);
    CHECK(server.tools.size() == 3);
    auto& app = Application::GetInstance();
    auto& codec = Board::GetInstance().codec;
    CHECK(codec.volume == 37 && codec.volume_set_calls == 0);
    auto& play = server.tools[0];
    auto args = play.defaults;
    CHECK(args["duration_ms"].value<int>() == 0);
    CHECK(args["level_percent"].value<int>() == 0);
    args.Set("kind", std::string("axel_f"));
    CHECK(play.callback(args).value().find("accepted") != std::string::npos);
    app.HandleGeneratedAudioEvent();
    size_t chunks = 0;
    while (app.audio_service_.IsLocalAudioActive()) {
        app.audio_service_.OutputChunk();
        ++chunks;
    }
    CHECK(chunks == 667);
    app.HandleGeneratedAudioEvent();
    CHECK(app.state == kDeviceStateIdle);
    CHECK(codec.volume == 37 && codec.volume_set_calls == 0);
    args.Set("duration_ms", 200);
    CHECK(play.callback(args).value().find("accepted") != std::string::npos);
    app.HandleGeneratedAudioEvent();
    chunks = 0;
    while (app.audio_service_.IsLocalAudioActive()) {
        app.audio_service_.OutputChunk();
        ++chunks;
    }
    CHECK(chunks == 20);
    app.HandleGeneratedAudioEvent();
    args.Set("kind", std::string("brass"));
    args.Set("duration_ms", 0);
    CHECK(play.callback(args).value().find("accepted") != std::string::npos);
    app.HandleGeneratedAudioEvent();
    chunks = 0;
    while (app.audio_service_.IsLocalAudioActive()) {
        app.audio_service_.OutputChunk();
        ++chunks;
    }
    CHECK(chunks == 500);
    app.HandleGeneratedAudioEvent();
    args.Set("level_percent", 20);
    CHECK(play.callback(args).value().find("accepted") != std::string::npos);
    args.Set("level_percent", 0);
    CHECK(play.callback(args).value().find("accepted") != std::string::npos);
    args.Set("level_percent", 100);
    CHECK(play.callback(args).value().find("accepted") != std::string::npos);
    args.Set("level_percent", -1);
    CHECK(play.callback(args).value().find("rejected") != std::string::npos);
    args.Set("level_percent", 101);
    CHECK(play.callback(args).value().find("rejected") != std::string::npos);
    args.Set("level_percent", 100);
    args.Set("frequency_hz", 54);
    CHECK(play.callback(args).value().find("rejected") != std::string::npos);
    args.Set("frequency_hz", 440);
    args.Set("duration_ms", 99);
    CHECK(play.callback(args).value().find("rejected") != std::string::npos);
    args.Set("duration_ms", -1);
    CHECK(play.callback(args).value().find("rejected") != std::string::npos);
    args.Set("duration_ms", 60001);
    CHECK(play.callback(args).value().find("rejected") != std::string::npos);
    args.Set("kind", std::string("not_a_kind"));
    CHECK(!play.callback(args).has_value());
    CHECK(codec.volume == 37 && codec.volume_set_calls == 0);
    for (const auto& kind : {std::string("saw"), std::string("ocean")}) {
        args.Set("kind", kind);
        args.Set("duration_ms", 100);
        args.Set("frequency_hz", 440);
        args.Set("level_percent", 20);
        CHECK(play.callback(args).value().find("accepted") != std::string::npos);
        app.HandleGeneratedAudioEvent();
        size_t count = 0;
        while (app.audio_service_.IsLocalAudioActive()) { app.audio_service_.OutputChunk(); ++count; }
        CHECK(count == 10);
        app.HandleGeneratedAudioEvent();
    }
    std::puts("actual MCP registration/callback defaults and tune cap passed (fake tool host)");
}
