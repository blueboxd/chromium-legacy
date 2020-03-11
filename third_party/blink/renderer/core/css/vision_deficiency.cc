// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/vision_deficiency.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

AtomicString CreateFilterDataUrl(AtomicString piece) {
  // clang-format off
  AtomicString url =
      "data:image/svg+xml,"
        "<svg xmlns=\"http://www.w3.org/2000/svg\">"
          "<filter id=\"f\">" +
            piece +
          "</filter>"
        "</svg>"
      "#f";
  // clang-format on
  return url;
}

AtomicString CreateVisionDeficiencyFilterUrl(
    VisionDeficiency vision_deficiency) {
  switch (vision_deficiency) {
    case VisionDeficiency::kAchromatomaly:
      return CreateFilterDataUrl(
          "<feColorMatrix values=\""
          "0.618 0.320 0.062 0.000 0.000 "
          "0.163 0.775 0.062 0.000 0.000 "
          "0.163 0.320 0.516 0.000 0.000 "
          "0.000 0.000 0.000 1.000 0.000 "
          "\"/>");
    case VisionDeficiency::kAchromatopsia:
      return CreateFilterDataUrl(
          "<feColorMatrix values=\""
          "0.299 0.587 0.114 0.000 0.000 "
          "0.299 0.587 0.114 0.000 0.000 "
          "0.299 0.587 0.114 0.000 0.000 "
          "0.000 0.000 0.000 1.000 0.000 "
          "\"/>");
    case VisionDeficiency::kBlurredVision:
      return CreateFilterDataUrl("<feGaussianBlur stdDeviation=\"2\"/>");
    case VisionDeficiency::kDeuteranomaly:
      return CreateFilterDataUrl(
          "<feColorMatrix values=\""
          "0.800 0.200 0.000 0.000 0.000 "
          "0.258 0.742 0.000 0.000 0.000 "
          "0.000 0.142 0.858 0.000 0.000 "
          "0.000 0.000 0.000 1.000 0.000 "
          "\"/>");
    case VisionDeficiency::kDeuteranopia:
      return CreateFilterDataUrl(
          "<feColorMatrix values=\""
          "0.625 0.375 0.000 0.000 0.000 "
          "0.700 0.300 0.000 0.000 0.000 "
          "0.000 0.300 0.700 0.000 0.000 "
          "0.000 0.000 0.000 1.000 0.000 "
          "\"/>");
    case VisionDeficiency::kProtanomaly:
      return CreateFilterDataUrl(
          "<feColorMatrix values=\""
          "0.817 0.183 0.000 0.000 0.000 "
          "0.333 0.667 0.000 0.000 0.000 "
          "0.000 0.125 0.875 0.000 0.000 "
          "0.000 0.000 0.000 1.000 0.000 "
          "\"/>");
    case VisionDeficiency::kProtanopia:
      return CreateFilterDataUrl(
          "<feColorMatrix values=\""
          "0.567 0.433 0.000 0.000 0.000 "
          "0.558 0.442 0.000 0.000 0.000 "
          "0.000 0.242 0.758 0.000 0.000 "
          "0.000 0.000 0.000 1.000 0.000 "
          "\"/>");
    case VisionDeficiency::kTritanomaly:
      return CreateFilterDataUrl(
          "<feColorMatrix values=\""
          "0.967 0.033 0.000 0.000 0.000 "
          "0.000 0.733 0.267 0.000 0.000 "
          "0.000 0.183 0.817 0.000 0.000 "
          "0.000 0.000 0.000 1.000 0.000 "
          "\"/>");
    case VisionDeficiency::kTritanopia:
      return CreateFilterDataUrl(
          "<feColorMatrix values=\""
          "0.950 0.050 0.000 0.000 0.000 "
          "0.000 0.433 0.567 0.000 0.000 "
          "0.000 0.475 0.525 0.000 0.000 "
          "0.000 0.000 0.000 1.000 0.000 "
          "\"/>");
    case VisionDeficiency::kNoVisionDeficiency:
      NOTREACHED();
      return "";
  }
}

}  // namespace blink
