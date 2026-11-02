// SPDX-License-Identifier: MIT
#include "application.h"
#include "local_audio_session.h"
#include <cassert>
#include <cstdio>
void Finish(Application& a) {
    while (a.audio_service_.IsLocalAudioActive()) a.audio_service_.OutputChunk();
    a.HandleGeneratedAudioEvent();
}
void Start(Application& a) {
    tone_noise::Parameters p;
    p.duration_ms = 100;
    assert(a.RequestGeneratedAudio(p).find("accepted") != std::string::npos);
    a.HandleGeneratedAudioEvent();
    assert(a.generated_audio_session_ && !a.audio_service_.mic);
}
int main() {
    { local_audio::InputSpeechBoundary boundary;
      const std::vector<int16_t> noise(480, 2000), quiet(480, 0);
      for (int i=0;i<10;++i) assert(!boundary.Observe(noise.data(),noise.size()));
      for (int i=0;i<4;++i) assert(!boundary.Observe(quiet.data(),quiet.size()));
      assert(!boundary.Observe(noise.data(),noise.size()));
      assert(boundary.Observe(noise.data(),noise.size()));
      assert(noise[0]==2000 && quiet[0]==0); }

    { Application a; Start(a); Finish(a);
      assert(a.state == kDeviceStateIdle && !a.audio_service_.mic);
      a.Pump(); assert(a.state == kDeviceStateListening && a.audio_service_.mic);
      assert(a.protocol_->starts == 1 && a.local_awaiting_user_);
      a.HandleLocalUserText("new request", a.generated_audio_epoch_);
      assert(!a.local_awaiting_user_); }
    { Application a; a.last_recognized_text_ = local_audio::TextFingerprint("play a tone");
      a.last_recognized_turn_ = a.recognized_user_turn_; Start(a); Finish(a); a.Pump();
      a.HandleLocalUserText("PLAY, A TONE!", a.generated_audio_epoch_);
      assert(a.local_awaiting_user_);
      a.HandleLocalUserText("next question", a.generated_audio_epoch_ - 1);
      assert(a.local_awaiting_user_);
      a.HandleLocalUserText("next question", a.generated_audio_epoch_);
      assert(!a.local_awaiting_user_); }
    { Application a; a.last_recognized_text_ = local_audio::TextFingerprint("play a tone");
      a.last_recognized_turn_ = a.recognized_user_turn_; Start(a); Finish(a); a.Pump();
      a.ObserveLocalInputActivity(std::vector<int16_t>(1920, 0));
      a.HandleLocalUserText("play a tone", a.generated_audio_epoch_);
      assert(a.local_awaiting_user_);
      a.ObserveLocalInputActivity(std::vector<int16_t>(960, 2000));
      a.HandleLocalUserText("play a tone", a.generated_audio_epoch_);
      assert(!a.local_awaiting_user_); }
    { Application a; a.last_recognized_text_ = local_audio::TextFingerprint("play a tone");
      a.last_recognized_turn_ = a.recognized_user_turn_; Start(a);
      a.ObserveLocalVoiceActivity();
      Finish(a); a.Pump();
      a.HandleLocalUserText("play a tone", a.generated_audio_epoch_);
      assert(a.local_awaiting_user_);
      a.ObserveLocalVoiceActivity();
      a.HandleLocalUserText("play a tone", a.generated_audio_epoch_);
      assert(!a.local_awaiting_user_); }
    { Application a; tone_noise::Playback other; tone_noise::Parameters p; p.duration_ms=100;
      auto request=a.RequestLocalPlayback(other,[&](uint32_t id,uint32_t rate) {
          return other.Request(p,rate,id);
      });
      assert(request.find("accepted")!=std::string::npos);
      const auto pending_id=a.generated_request_id_;
      assert(a.RequestGeneratedAudio(p).find("rejected")!=std::string::npos);
      assert(a.generated_command_pending_ && a.local_command_source_==&other &&
             a.generated_request_id_==pending_id);
      assert(a.GeneratedAudioStatus().find("accepted")==std::string::npos);
      assert(a.StopGeneratedAudio().find("not_owned")!=std::string::npos);
      assert(a.generated_command_pending_ && a.generated_request_id_==pending_id);
      a.HandleLocalPlaybackEvent();assert(other.active() && a.local_active_source_==&other);
      assert(a.RequestGeneratedAudio(p).find("rejected")!=std::string::npos);
      assert(a.GeneratedAudioStatus().find("playing")==std::string::npos);
      assert(a.StopGeneratedAudio().find("not_owned")!=std::string::npos);
      a.HandleLocalPlaybackEvent();assert(other.active() && !a.generated_stop_pending_);
      a.StopLocalPlayback();a.HandleLocalPlaybackEvent();Finish(a);a.Pump();
      assert(!other.active()); }
    { Application a; Start(a); a.StopGeneratedAudio(); a.HandleGeneratedAudioEvent();
      Finish(a); a.Pump(); assert(a.state == kDeviceStateListening && a.protocol_->starts == 1); }
    { Application a; Start(a); a.generated_playback_.Fail(); Finish(a); a.Pump();
      assert(a.state == kDeviceStateIdle && !a.audio_service_.mic && a.audio_service_.wake);
      assert(a.protocol_->starts == 0 && a.GeneratedAudioStatus().find("error") != std::string::npos); }
    { Application a; Start(a); Finish(a); Start(a); a.Pump();
      assert(a.protocol_->starts == 0 && a.generated_audio_session_); Finish(a); a.Pump();
      assert(a.protocol_->starts == 1); }
    { Application a; a.protocol_->open = false; Start(a); Finish(a);
      auto fn = std::move(a.tasks.front()); a.tasks.pop_front(); fn();
      assert(a.state == kDeviceStateConnecting);
      a.generated_resume_allowed_ = false; a.Pump();
      assert(a.state == kDeviceStateIdle && a.protocol_->opens == 0 && !a.audio_service_.mic); }
    { Application a; a.protocol_->open = false; Start(a); Finish(a); a.Pump();
      assert(a.protocol_->opens == 1 && a.protocol_->starts == 1 && a.audio_service_.mic); }
    { Application a; a.last_error_message_ = "synthetic error"; Start(a); Finish(a); a.Pump();
      assert(a.state == kDeviceStateIdle && a.protocol_->starts == 0); }
    { Application a; Start(a); a.generated_resume_allowed_ = false; a.StopGeneratedAudio();
      a.HandleGeneratedAudioEvent(); Finish(a); a.Pump();
      assert(a.state == kDeviceStateIdle && a.protocol_->starts == 0 && !a.audio_service_.mic); }
    std::puts("Resume: completion/stop, new user admission, stale epochs, replacement and failure cancellation passed");
    return 0;
}
