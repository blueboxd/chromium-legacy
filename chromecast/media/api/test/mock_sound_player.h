// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_TEST_MOCK_SOUND_PLAYER_H_
#define CHROMECAST_MEDIA_API_TEST_MOCK_SOUND_PLAYER_H_

#include <memory>
#include <string>

#include "chromecast/media/api/sound_player.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockSoundPlayer : public SoundPlayer {
 public:
  MockSoundPlayer();
  ~MockSoundPlayer() override;

  MOCK_METHOD(void, Play, (int, int, bool), (override));
  MOCK_METHOD(void,
              PlayAudioData,
              (int, scoped_refptr<AudioData>, bool),
              (override));
  MOCK_METHOD(void,
              PlayAtTime,
              (int, int64_t, media::AudioChannel),
              (override));
  MOCK_METHOD(void, Stop, (int), (override));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_TEST_MOCK_SOUND_PLAYER_H_
