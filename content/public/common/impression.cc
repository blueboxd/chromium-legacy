// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/impression.h"

#include "third_party/blink/public/platform/web_security_origin.h"

namespace content {

Impression::Impression() = default;

Impression::Impression(const Impression& other) = default;

Impression& Impression::operator=(const Impression& other) = default;

Impression::~Impression() = default;

}  // namespace content
