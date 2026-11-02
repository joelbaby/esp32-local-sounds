#!/usr/bin/env python3
"""Replay the installed production STT/TTS/error callback branches with fake host.

Uses XiaoZhi's existing cJSON dependency; never downloads or copies its cache.
"""
from pathlib import Path
import argparse
import subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('checkout',type=Path)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
app=(a.checkout/'main/application.cc').read_text()
start=app.index('        } else if (strcmp(type->valuestring, "tts") == 0)')
end=app.index('        } else if (strcmp(type->valuestring, "llm") == 0)',start)
branches=app[start:end].replace('        } else if','        if',1)+'        }'
start=app.index('    protocol_->OnNetworkError(')
end=app.index('    });',start)+7
error_callback=app[start:end]
# Extend the same fake platform used by adapter tests. Production callback bodies
# below are extracted verbatim, so their guards are exercised at receive/execute.
support=(ROOT/'tests/support/application.h').read_text()
support=support.replace('struct Protocol {','struct Protocol {\n    std::function<void(const std::string&)> error_callback;\n    void OnNetworkError(std::function<void(const std::string&)> fn) { error_callback=std::move(fn); }')
support=support.replace('class Application {','struct cJSON;\nclass Application {')
support=support.replace('    void HandleLocalPlaybackEvent();','    void Incoming(const cJSON*);\n    void InstallErrorCallback();\n    bool aborted_=false;\n    void HandleLocalPlaybackEvent();')
(a.output/'application.h').write_text(support)
(a.output/'board.h').write_text((ROOT/'tests/support/board.h').read_text().replace('struct Board {','''struct Display {
    unsigned assistant=0,user=0;
    void AddTextGlyphs(const std::vector<int>&,uint8_t) {}
    void SetChatMessage(const char* role,const char*) { if(std::string(role)=="assistant") ++assistant; else ++user; }
};
struct Board {
    Display display;
    Display* GetDisplay() { return &display; }
'''))
fixture='''
#include "application.h"
#include "board.h"
#include "cJSON.h"
#include "local_audio_session.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#define ESP_LOGI(...) ((void)0)
#define MAIN_EVENT_ERROR 2
using TextGlyph=int;
struct TextGlyphPayload { static bool Parse(const cJSON*,std::vector<int>&,uint8_t&) {return false;} };
void Application::Incoming(const cJSON* root) {
    auto display=Board::GetInstance().GetDisplay();
    auto type=cJSON_GetObjectItem(root,"type");
'''+branches+'''\n}
void Application::InstallErrorCallback() {
'''+error_callback+'''\n}
void Incoming(Application& a,const char* json) {
    auto* root=cJSON_Parse(json);assert(root);a.Incoming(root);cJSON_Delete(root);
}
void Start(Application& a) {
    tone_noise::Parameters p;p.duration_ms=100;
    a.RequestGeneratedAudio(p);a.HandleLocalPlaybackEvent();
}
void Finish(Application& a) {
    while(a.audio_service_.IsLocalAudioActive())a.audio_service_.OutputChunk();
    a.HandleLocalPlaybackEvent();a.Pump();
}
int main(){
    auto& display=Board::GetInstance().display;
    {Application a;
     Incoming(a,R"({"type":"tts","state":"start"})");
     Incoming(a,R"({"type":"tts","state":"sentence_start","text":"stale"})");
     Start(a);a.Pump();assert(a.generated_audio_session_ && display.assistant==0);
     Finish(a);assert(a.local_awaiting_user_ && a.state==kDeviceStateListening);
     Incoming(a,R"({"type":"tts","state":"start"})");
     Incoming(a,R"({"type":"tts","state":"sentence_start","text":"old"})");
     Incoming(a,R"({"type":"tts","state":"stop"})");a.Pump();
     assert(a.state==kDeviceStateListening && display.assistant==0);
     // A fresh STT queued before its complete TTS burst must clear the guard at
     // execution, even though all four messages arrived before the loop ran.
     Incoming(a,R"({"type":"stt","text":"new user question"})");
     Incoming(a,R"({"type":"tts","state":"start"})");
     Incoming(a,R"({"type":"tts","state":"sentence_start","text":"new answer"})");
     Incoming(a,R"({"type":"tts","state":"stop"})");a.Pump();
     assert(!a.local_awaiting_user_ && display.assistant==1);
    }
    {Application a;
     a.last_recognized_text_=local_audio::TextFingerprint("play a tone");
     a.last_recognized_turn_=a.recognized_user_turn_;
     Start(a);Finish(a);auto before=display.assistant;
     Incoming(a,R"({"type":"stt","text":"play a tone"})");
     Incoming(a,R"({"type":"tts","state":"start"})");
     Incoming(a,R"({"type":"tts","state":"sentence_start","text":"late duplicate"})");
     a.Pump();assert(a.local_awaiting_user_ && display.assistant==before);
     a.ObserveLocalInputActivity(std::vector<int16_t>(1920,0));
     a.ObserveLocalInputActivity(std::vector<int16_t>(960,2000));
     Incoming(a,R"({"type":"stt","text":"play a tone"})");
     Incoming(a,R"({"type":"tts","state":"start"})");
     Incoming(a,R"({"type":"tts","state":"sentence_start","text":"fresh repeated request"})");
     Incoming(a,R"({"type":"tts","state":"stop"})");
     a.Pump();assert(!a.local_awaiting_user_ && display.assistant==before+1);
    }
    {Application a;a.InstallErrorCallback();Start(a);
     // Error callback can run synchronously under the command mutex: it must
     // only disable resume and post an event, never recursively lock it.
     {std::lock_guard<std::mutex> lock(a.generated_command_mutex_);
      a.protocol_->error_callback("synthetic network failure");}
     assert(!a.generated_resume_allowed_);
     a.StopLocalPlayback();a.HandleLocalPlaybackEvent();Finish(a);
     assert(a.state==kDeviceStateIdle && a.protocol_->starts==0 && !a.audio_service_.mic);
    }
    std::puts("Actual callback branches: stale queued TTS, post-resume old TTS, fresh STT/TTS burst, synchronous error cancellation passed");
}
'''
# Fake SetDeviceState/SetListeningMode for TTS stop's configured mode.
fixture=fixture.replace('#include <cstdio>','#include <cstdio>\nconstexpr auto kListeningModeManualStop=static_cast<ListeningMode>(1);')
support=(a.output/'application.h').read_text().replace('    bool aborted_=false;','    bool aborted_=false;\n    ListeningMode listening_mode_=kListeningModeAutoStop;')
(a.output/'application.h').write_text(support)
(a.output/'callbacks.cc').write_text(fixture)
cjson=a.checkout/'managed_components/espressif__cjson/cJSON'
subprocess.run(['cc','-I',str(cjson),'-c',str(cjson/'cJSON.c'),'-o',str(a.output/'cjson.o')],check=True)
subprocess.run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Werror',
 '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(a.output),'-I',str(ROOT/'include'),'-I',str(cjson),
 str(a.output/'callbacks.cc'),str(ROOT/'integrations/xiaozhi/generated_audio_adapter.cc'),
 str(ROOT/'integrations/xiaozhi/local_audio_adapter.cc'),str(ROOT/'src/tone_noise_generator.cc'),
 str(a.output/'cjson.o'),'-o',str(a.output/'callbacks')],check=True)
subprocess.run([str(a.output/'callbacks')],check=True,timeout=10)
