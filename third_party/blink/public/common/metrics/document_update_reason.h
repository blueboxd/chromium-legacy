// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_DOCUMENT_UPDATE_REASON_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_DOCUMENT_UPDATE_REASON_H_

#include <stdint.h>

namespace blink {

// An enum class with the reasons for requesting a document lifecycle update
// or an update to style and layout. Used to attribute work in the renderer
// to particular code modules in metrics, to guide work on improving the
// platform and to help identify the cause of performance regressions or
// improvements.

enum class DocumentUpdateReason { kBeginMainFrame, kTest, kOther };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_DOCUMENT_UPDATE_REASON_H_
