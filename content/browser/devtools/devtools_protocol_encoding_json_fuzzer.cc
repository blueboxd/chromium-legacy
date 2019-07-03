// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol_encoding.h"

#include <string>
#include "third_party/inspector_protocol/encoding/encoding.h"

namespace content {
namespace {
using ::inspector_protocol_encoding::span;
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string cbor;
  content::ConvertJSONToCBOR(span<uint8_t>(data, size), &cbor);
  return 0;
}
}  // namespace content
