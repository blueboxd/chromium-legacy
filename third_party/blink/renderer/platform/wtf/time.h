// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TIME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TIME_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

// Returns the current UTC time in seconds, counted from January 1, 1970.
// Precision varies depending on platform but is usually as good or better
// than a millisecond.
WTF_EXPORT double CurrentTime();

// Same thing, in milliseconds.
inline double CurrentTimeMS() {
  return CurrentTime() * 1000.0;
}

}  // namespace WTF

using WTF::CurrentTime;
using WTF::CurrentTimeMS;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TIME_H_
