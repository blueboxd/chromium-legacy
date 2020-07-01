// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder.h"

namespace media {

Decoder::Decoder() = default;

Decoder::~Decoder() = default;

bool Decoder::IsPlatformDecoder() const {
  return false;
}

bool Decoder::SupportsDecryption() const {
  return false;
}

}  // namespace media
