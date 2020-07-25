// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_IDENTIFIABILITY_STUDY_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_IDENTIFIABILITY_STUDY_HELPER_H_

#include <stdint.h>

#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_participation.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

// Text operations supported on different canvas types; the intent is to use
// these values (and any input supplied to these operations) to build a running
// hash that reprensents the sequence of text operations performed on the
// canvas. A hash of all other canvas operations is maintained by hashing the
// serialized PaintOps produced by the canvas in CanvasResourceProvider.
//
// If a canvas method to exfiltrate the canvas buffer is called by a script
// (getData(), etc.), this hash will be uploaded to UKM along with a hash of the
// canvas buffer data.
//
// **Don't renumber after the privacy budget study has started to ensure
// consistency.**
enum class CanvasOps {
  // CanvasRenderingContext2D / OffscreenCanvasRenderingContext2D methods.
  kSetFont,
  kFillText,
  kStrokeText,
};

// A helper class to simplify maintaining the current text digest for the canvas
// context. An operation count is also maintained to limit the performance
// impact of the study.
class IdentifiabilityStudyHelper {
 public:
  template <typename... Ts>
  void MaybeUpdateBuilder(Ts... tokens) {
    constexpr int kMaxOperations = 1 << 20;
    if (!IsUserInIdentifiabilityStudy() || operation_count_ > kMaxOperations) {
      return;
    }
    AddTokens(tokens...);
    operation_count_++;
  }

  IdentifiableToken GetToken() { return builder_.GetToken(); }

 private:
  // Note that primitives are implicitly converted to IdentifiableTokens
  template <typename... Ts>
  void AddTokens(IdentifiableToken token, Ts... args) {
    builder_.AddToken(token);
    AddTokens(args...);
  }
  void AddTokens() {}

  IdentifiableTokenBuilder builder_;
  int operation_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_IDENTIFIABILITY_STUDY_HELPER_H_
