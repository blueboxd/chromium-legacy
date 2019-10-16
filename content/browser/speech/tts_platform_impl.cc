// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_platform_impl.h"

#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#endif

#include <string>

namespace content {

// static
TtsPlatform* TtsPlatform::GetInstance() {
#if defined(OS_CHROMEOS)
  // Chrome TTS platform has chrome/ dependencies.
  return GetContentClient()->browser()->GetTtsPlatform();
#elif defined(OS_FUCHSIA)
  // There is no platform TTS definition for Fuchsia.
  return nullptr;
#else
  return TtsPlatformImpl::GetInstance();
#endif
}

bool TtsPlatformImpl::LoadBuiltInTtsEngine(BrowserContext* browser_context) {
  return false;
}

void TtsPlatformImpl::WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                                  const VoiceData& voice_data) {
}

std::string TtsPlatformImpl::GetError() {
  return error_;
}

void TtsPlatformImpl::ClearError() {
  error_ = std::string();
}

void TtsPlatformImpl::SetError(const std::string& error) {
  error_ = error;
}

}  // namespace content
