// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/services/assistant/fake_client.h"
#include "chromeos/services/assistant/public/features.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

namespace {

class FakeAssistantClient : public FakeClient {
 public:
  FakeAssistantClient() = default;
  ~FakeAssistantClient() override = default;

  // FakeClient overrides:
  void RequestAudioStreamFactory(
      mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) override {
    if (!fake_stream_factory_.receiver_.is_bound())
      fake_stream_factory_.receiver_.Bind(std::move(receiver));
  }

 private:
  audio::FakeStreamFactory fake_stream_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeAssistantClient);
};

}  // namespace

class AudioInputImplTest : public testing::Test,
                           public assistant_client::AudioInput::Observer {
 public:
  AudioInputImplTest() {
    // Enable DSP feature flag.
    scoped_feature_list_.InitAndEnableFeature(features::kEnableDspHotword);

    chromeos::PowerManagerClient::InitializeFake();
    audio_input_impl_ = std::make_unique<AudioInputImpl>(
        &fake_assistant_client_, FakePowerManagerClient::Get(),
        "fake-device-id", "fake-hotword-device-id");

    audio_input_impl_->AddObserver(this);
  }

  ~AudioInputImplTest() override {
    audio_input_impl_->RemoveObserver(this);
    audio_input_impl_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  bool GetRecordingStatus() const {
    return audio_input_impl_->IsRecordingForTesting();
  }

  // assistant_client::AudioInput::Observer overrides:
  void OnAudioBufferAvailable(const assistant_client::AudioBuffer& buffer,
                              int64_t timestamp) override {}
  void OnAudioError(assistant_client::AudioInput::Error error) override {}
  void OnAudioStopped() override {}

 protected:
  void ReportLidEvent(chromeos::PowerManagerClient::LidState state) {
    FakePowerManagerClient::Get()->SetLidState(state,
                                               base::TimeTicks::UnixEpoch());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeAssistantClient fake_assistant_client_;
  std::unique_ptr<AudioInputImpl> audio_input_impl_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputImplTest);
};

TEST_F(AudioInputImplTest, StopRecordingWhenLidClosed) {
  // Trigger a lid open event.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());

  // Trigger a lid closed event.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::CLOSED);
  EXPECT_FALSE(GetRecordingStatus());

  // Trigger a lid open event again.
  ReportLidEvent(chromeos::PowerManagerClient::LidState::OPEN);
  EXPECT_TRUE(GetRecordingStatus());
}

}  // namespace assistant
}  // namespace chromeos
