#!/usr/bin/env python3
"""Compile actual installed shared AudioService/driver methods with fake RTOS/I2S.

No sound algorithms or optional feature headers are linked in this test.
"""
from pathlib import Path
import argparse
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('checkout', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

def method(text, name):
    start = text.index(name)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

src = (a.checkout/'main/audio/audio_service.cc').read_text()
names = ['bool AudioService::ControlLocalAudio(', 'void AudioService::StopLocalAudio(',
         'void AudioService::ReleaseLocalAudio(', 'bool AudioService::IsLocalAudioActive(',
         'bool AudioService::LocalAudioFailed(', 'std::string AudioService::LocalAudioStatus(',
         'bool AudioService::IsPlaybackDrainedLocked(', 'bool AudioService::MarkPlaybackDrainedLocked(',
         'bool AudioService::PushPacketToDecodeQueue(', 'void AudioService::Stop()',
         'void AudioService::AudioOutputTask(']
fixture = r'''
#include "local_audio_source.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#define AS_EVENT_AUDIO_TESTING_RUNNING 1
#define AS_EVENT_WAKE_WORD_RUNNING 2
#define AS_EVENT_AUDIO_PROCESSOR_RUNNING 4
#define MAX_DECODE_PACKETS_IN_QUEUE 20
#define AUDIO_POWER_CHECK_INTERVAL_MS 1000
#define ESP_LOGW(...) ((void)0)
inline void esp_timer_stop(int) {}
inline void esp_timer_start_periodic(int,int) {}
inline void xEventGroupSetBits(int,int) {}
struct AudioStreamPacket {};
struct Source : local_audio::Source {
    bool running = false, error = false, empty = false;
    int commits = 0, renders = 0, stops = 0;
    size_t Render(int16_t* out,size_t n) override { assert(n == 240); ++renders; out[0]=123; return empty ? 0 : n; }
    void Commit() override { ++commits; running = false; }
    void Stop() override { ++stops; running = false; }
    void Fail() override { error = true; running = false; }
    bool active() const override { return running; }
    bool failed() const override { return error; }
    std::string StatusJson() const override { return error ? "error" : "okay"; }
};
struct Codec {
    bool success = true;
    int calls = 0;
    std::function<void()> during;
    bool output_enabled() { return false; }
    void EnableOutput(bool) { assert(false && "generated branch bypassed checked activation"); }
    bool OutputGeneratedData(std::vector<int16_t>& pcm) {
        ++calls; assert(pcm.size() == 240); if(during) during(); return success;
    }
    void OutputData(std::vector<int16_t>&) { assert(false); }
};
struct AudioService {
    bool ControlLocalAudio(local_audio::Source&, const std::function<bool()>&);
    void StopLocalAudio(local_audio::Source* expected=nullptr);
    void ReleaseLocalAudio(local_audio::Source* expected=nullptr);
    bool IsLocalAudioActive(); bool LocalAudioFailed();
    std::string LocalAudioStatus(const local_audio::Source&);
    bool IsPlaybackDrainedLocked() const; bool MarkPlaybackDrainedLocked();
    bool PushPacketToDecodeQueue(std::unique_ptr<AudioStreamPacket>,bool wait=false);
    void Stop(); void AudioOutputTask();
    Codec codec; Codec* codec_=&codec;
    int audio_power_timer_=0,event_group_=0;
    std::atomic<bool> service_stopped_{false};
    std::mutex audio_queue_mutex_; std::condition_variable audio_queue_cv_;
    local_audio::Source* local_source_=nullptr; uint32_t local_source_epoch_=0;
    uint32_t playback_generation_=0;
    bool decode_in_flight_=false,output_in_flight_=false,playback_drained_notified_=true;
    std::deque<int> audio_encode_queue_,audio_testing_queue_;
    std::deque<std::unique_ptr<AudioStreamPacket>> audio_decode_queue_;
    struct Task { uint32_t playback_id=0,media_position_ms=0; std::vector<int16_t> pcm; };
    std::deque<Task> audio_playback_queue_;
    struct { int playback_count=0; } debug_statistics_;
    std::chrono::steady_clock::time_point last_output_time_;
    struct {
        std::function<void()> on_local_audio_finished,on_playback_drained;
        std::function<void(uint32_t,uint32_t)> on_playback_progress;
    } callbacks_;
};
'''
checks = r'''
int main() {
    for(bool success : {true,false}) {
        AudioService s; Source first, other;
        assert(s.PushPacketToDecodeQueue(std::make_unique<AudioStreamPacket>()));
        assert(s.ControlLocalAudio(first,[&]{first.running=true;return true;}));
        assert(s.audio_decode_queue_.empty() && s.playback_generation_==1);
        assert(!s.ControlLocalAudio(other,[&]{other.running=true;return true;}));
        assert(!other.running);
        assert(!s.PushPacketToDecodeQueue(std::make_unique<AudioStreamPacket>()));
        s.codec.success=success;
        int finished=0;
        s.codec.during=[&] {
            assert(s.IsLocalAudioActive()); // In-flight ownership cannot be released.
            s.ReleaseLocalAudio(&first); assert(s.local_source_==&first);
            s.StopLocalAudio(&other); assert(first.stops==0);
        };
        s.callbacks_.on_local_audio_finished=[&] {
            ++finished;
            assert(!s.IsLocalAudioActive());
            assert(!s.IsPlaybackDrainedLocked()); // Remains owned until app release.
            s.service_stopped_=true; s.audio_queue_cv_.notify_all();
        };
        std::thread output([&]{s.AudioOutputTask();}); output.join();
        assert(finished==1 && first.renders==1 && s.codec.calls==1);
        assert(first.commits==(success ? 1:0) && first.error==!success);
        assert(s.LocalAudioFailed()==!success);
        s.ReleaseLocalAudio(&other); assert(s.local_source_==&first);
        s.ReleaseLocalAudio(&first); assert(s.IsPlaybackDrainedLocked());
    }
    { AudioService s; Source source;
      assert(s.ControlLocalAudio(source,[&]{source.running=true;return true;}));
      s.StopLocalAudio(&source); assert(source.stops==1 && !s.IsLocalAudioActive());
      s.ReleaseLocalAudio(&source); assert(!s.local_source_); }
    { AudioService s; Source source; source.empty=true;
      assert(s.ControlLocalAudio(source,[&]{source.running=true;return true;}));
      s.callbacks_.on_local_audio_finished=[&]{s.service_stopped_=true;s.audio_queue_cv_.notify_all();};
      std::thread output([&]{s.AudioOutputTask();});output.join();
      assert(source.error && source.renders==1 && source.commits==0 && s.codec.calls==0); }
    { AudioService s; Source source; int notifications=0;
      assert(s.ControlLocalAudio(source,[&]{source.running=true;return true;}));
      s.callbacks_.on_local_audio_finished=[&]{
          if(++notifications==2){s.service_stopped_=true;s.audio_queue_cv_.notify_all();}};
      s.codec.during=[&]{if(s.codec.calls==1){
          s.Stop();s.service_stopped_=false;
          assert(s.ControlLocalAudio(source,[&]{source.running=true;source.error=false;return true;}));
      }};
      std::thread output([&]{s.AudioOutputTask();});output.join();
      assert(source.renders==2 && source.commits==1 && notifications==2 && !source.error); }
    { AudioService s; Source source;
      assert(s.ControlLocalAudio(source,[&]{source.running=true;return true;}));
      s.Stop(); assert(source.error && !s.local_source_);
      assert(!s.ControlLocalAudio(source,[]{return true;})); }
}
'''
(a.output/'shared.cc').write_text(fixture+'\n'.join(method(src,n) for n in names)+checks)
subprocess.run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Werror',
                '-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(ROOT/'include'),
                str(a.output/'shared.cc'),'-o',str(a.output/'shared')],check=True)
subprocess.run([str(a.output/'shared')],check=True,timeout=10)

src=(a.checkout/'main/audio/codecs/no_audio_codec.cc').read_text()
fixture=r'''
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
using esp_err_t=int;
constexpr int ESP_OK=0;
static int64_t fake_time=0, write_advance=0;
int64_t esp_timer_get_time(){return fake_time;}
static size_t accepted=960, requested=0, allocation=0;
static int result=0,calls=0,enable_result=0,enable_calls=0;
static int64_t enable_advance=0;
int i2s_channel_enable(int){++enable_calls;fake_time+=enable_advance;return enable_result;}
static bool allocation_failure=false;
static uint32_t wait_ms=0;
int i2s_channel_write(int,const void* pcm,size_t bytes,size_t* written,uint32_t wait){
    ++calls; fake_time+=write_advance; requested=bytes; wait_ms=wait; *written=accepted;
    assert(static_cast<const int32_t*>(pcm)[0]==16384000); return result;
}
namespace std { void* fake_malloc(size_t size) {
    allocation=size; return allocation_failure ? nullptr : ::malloc(size);
} }
#define malloc fake_malloc
struct NoAudioCodec {
    std::timed_mutex data_if_mutex_;
    int output_volume_=50,tx_handle_=1;
    bool output_enabled_=false;
    bool OutputGeneratedData(std::vector<int16_t>&);
};
'''
checks=r'''
#undef malloc
int main(){
    NoAudioCodec codec;
    std::vector<int16_t> pcm(240,1000);
    assert(codec.OutputGeneratedData(pcm));
    assert(codec.output_enabled_ && enable_calls==1);
    assert(calls==1 && allocation==960 && requested==960 && wait_ms<=100);
    codec.output_enabled_=false;enable_result=1;auto before_enable=calls;
    assert(!codec.OutputGeneratedData(pcm) && calls==before_enable && !codec.output_enabled_);
    enable_result=0;enable_advance=101000;
    assert(!codec.OutputGeneratedData(pcm) && calls==before_enable);
    enable_advance=0;assert(codec.OutputGeneratedData(pcm) && codec.output_enabled_);
    accepted=956; assert(!codec.OutputGeneratedData(pcm));
    accepted=0; assert(!codec.OutputGeneratedData(pcm));
    accepted=960; write_advance=101000; assert(!codec.OutputGeneratedData(pcm));
    write_advance=0; result=1; assert(!codec.OutputGeneratedData(pcm));
    result=0; allocation_failure=true; auto old=calls;
    assert(!codec.OutputGeneratedData(pcm) && calls==old);
    allocation_failure=false; pcm.resize(241); assert(!codec.OutputGeneratedData(pcm));
    pcm.resize(240);
    std::mutex gate; std::condition_variable cv; bool locked=false;
    std::thread holder([&]{std::lock_guard<std::timed_mutex> lock(codec.data_if_mutex_);
        {std::lock_guard<std::mutex> g(gate);locked=true;}cv.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(180));});
    {std::unique_lock<std::mutex> g(gate);cv.wait(g,[&]{return locked;});}
    codec.output_enabled_=false;
    const auto previous_enable_calls=enable_calls;
    auto start=std::chrono::steady_clock::now();
    assert(!codec.OutputGeneratedData(pcm));
    assert(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(170));
    assert(enable_calls==previous_enable_calls && !codec.output_enabled_);
    holder.join();
}
'''
fixture=fixture.replace('#include <chrono>','#include <chrono>\n#include <condition_variable>')
(a.output/'driver.cc').write_text(fixture+method(src,'bool NoAudioCodec::OutputGeneratedData(')+checks)
subprocess.run(['c++','-std=c++17','-pthread','-Wall','-Wextra','-Werror',
                '-fsanitize=undefined','-fno-sanitize-recover=all',str(a.output/'driver.cc'),
                '-o',str(a.output/'driver')],check=True)
subprocess.run([str(a.output/'driver')],check=True,timeout=10)
print('Actual shared ownership/output methods and full-buffer driver: success, short/error, allocation, stop and lock deadline passed.')
