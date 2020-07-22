// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_service_util.h"

#include "base/feature_list.h"
#include "content/public/common/content_features.h"

static base::Optional<bool> g_force_audio_service_sandbox;

bool IsAudioServiceSandboxEnabled() {
  return g_force_audio_service_sandbox.value_or(
      base::FeatureList::IsEnabled(features::kAudioServiceSandbox));
}

// If |force_audio_service_sandbox| is:
// * null: use the default audio-service sandbox configuration (varies per
//         platform)
// * true: enable the audio-service sandbox, regardless of default settings.
// * false: disable the audio-service sandbox, regardless of default settings.
void SetForceAudioServiceSandboxed(
    const base::Optional<bool>& force_audio_service_sandbox) {
  g_force_audio_service_sandbox = force_audio_service_sandbox;
}
