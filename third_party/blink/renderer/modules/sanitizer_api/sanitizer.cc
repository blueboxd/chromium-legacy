// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

Sanitizer* Sanitizer::Create(ExceptionState& exception_state) {
  return MakeGarbageCollected<Sanitizer>();
}

Sanitizer::Sanitizer() = default;

Sanitizer::~Sanitizer() = default;

String Sanitizer::saneStringFrom(const String& input) {
  String sanitizedString = input;
  return sanitizedString;
}

}  // namespace blink
