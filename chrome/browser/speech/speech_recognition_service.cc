// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_service.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/services/speech/buildflags.h"
#include "content/public/browser/service_process_host.h"

namespace speech {

constexpr base::TimeDelta kIdleProcessTimeout = base::TimeDelta::FromSeconds(5);

SpeechRecognitionService::SpeechRecognitionService() = default;
SpeechRecognitionService::~SpeechRecognitionService() = default;

void SpeechRecognitionService::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  LaunchIfNotRunning();
  speech_recognition_service_->BindContext(std::move(receiver));
}

void SpeechRecognitionService::LaunchIfNotRunning() {
  if (speech_recognition_service_.is_bound())
    return;

  content::ServiceProcessHost::Launch(
      speech_recognition_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_SPEECH_RECOGNITION_SERVICE_NAME)
  // Use the custom speech recognition sandbox type if the Speech On-Device API
  // is enabled. Otherwise, use the utility sandbox type.
#if BUILDFLAG(ENABLE_SODA)
          .WithSandboxType(service_manager::SandboxType::kSpeechRecognition)
#else
          .WithSandboxType(service_manager::SandboxType::kUtility)
#endif  // BUILDFLAG(ENABLE_SODA)
          .Pass());

  // Ensure that if the interface is ever disconnected (e.g. the service
  // process crashes) or goes idle for a short period of time -- meaning there
  // are no in-flight messages and no other interfaces bound through this
  // one -- then we will reset |remote|, causing the service process to be
  // terminated if it isn't already.
  speech_recognition_service_.reset_on_disconnect();
  speech_recognition_service_.reset_on_idle_timeout(kIdleProcessTimeout);
}

}  // namespace speech
