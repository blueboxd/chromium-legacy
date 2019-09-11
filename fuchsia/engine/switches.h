// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_SWITCHES_H_
#define FUCHSIA_ENGINE_SWITCHES_H_

namespace switches {

// Enables Widevine CDM support.
extern const char kEnableWidevine[];

// Enables PlayReady CDM and specifies the corresponding key system string.
extern const char kPlayreadyKeySystem[];

// Prevents the use of video codecs that are not hardware-accelerated.
extern const char
    kDisableSoftwareVideoDecoders[];  // "disable-software-video-decoders"

}  // namespace switches

#endif  // FUCHSIA_ENGINE_SWITCHES_H_
