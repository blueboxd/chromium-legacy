// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_OVERLAY_INTERSTITIAL_AD_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_OVERLAY_INTERSTITIAL_AD_DETECTOR_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT OverlayInterstitialAdDetector {
 public:
  OverlayInterstitialAdDetector() = default;
  ~OverlayInterstitialAdDetector() = default;

  void MaybeFireDetection(LocalFrame* main_frame);

 private:
  base::Optional<base::Time> last_detection_time_;
  bool done_detection_ = false;

  DISALLOW_COPY_AND_ASSIGN(OverlayInterstitialAdDetector);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_OVERLAY_INTERSTITIAL_AD_DETECTOR_H_
