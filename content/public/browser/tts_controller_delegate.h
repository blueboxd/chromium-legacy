// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_

#include <vector>

namespace content {

class TtsUtterance;
struct VoiceData;

// Allows embedders to access the current state of text-to-speech.
class TtsControllerDelegate {
 public:
  // Given an utterance and a vector of voices, return the
  // index of the voice that best matches the utterance.
  virtual int GetMatchingVoice(TtsUtterance* utterance,
                               std::vector<VoiceData>& voices) = 0;

  // Uses the user preferences to update the |rate|, |pitch| and |volume| for
  // a given |utterance|.
  virtual void UpdateUtteranceDefaultsFromPrefs(TtsUtterance* utterance,
                                                double* rate,
                                                double* pitch,
                                                double* volume) = 0;
 protected:
  virtual ~TtsControllerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_
