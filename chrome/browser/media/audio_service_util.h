// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_AUDIO_SERVICE_UTIL_H_
#define CHROME_BROWSER_MEDIA_AUDIO_SERVICE_UTIL_H_

#include "base/optional.h"

bool IsAudioServiceSandboxEnabled();

// If |force_audio_service_sandbox| is:
// * null: use the default audio-service sandbox configuration (varies per
//         platform)
// * true: enable the audio-service sandbox, regardless of default settings.
// * false: disable the audio-service sandbox, regardless of default settings.
void SetForceAudioServiceSandboxed(
    const base::Optional<bool>& force_audio_service_sandbox);

#endif  // CHROME_BROWSER_MEDIA_AUDIO_SERVICE_UTIL_H_
