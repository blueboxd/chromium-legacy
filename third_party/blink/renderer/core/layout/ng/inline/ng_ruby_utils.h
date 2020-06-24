// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_RUBY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_RUBY_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
class NGLineInfo;
class NGLogicalLineItems;
class NGPhysicalBoxFragment;
struct NGInlineItemResult;
struct NGLineHeightMetrics;

// Returns the logical bottom offset of the last line text, relative to
// |container| origin. This is used to decide ruby annotation box position.
//
// See NGBlockLayoutAlgorithm::LayoutRubyText().
LayoutUnit LastLineTextLogicalBottom(const NGPhysicalBoxFragment& container,
                                     LayoutUnit default_value);

// Returns the logical top offset of the first line text, relative to
// |container| origin. This is used to decide ruby annotation box position.
//
// See NGBlockLayoutAlgorithm::LayoutRubyText().
LayoutUnit FirstLineTextLogicalTop(const NGPhysicalBoxFragment& container,
                                   LayoutUnit default_value);

struct NGAnnotationOverhang {
  LayoutUnit start;
  LayoutUnit end;
};

// Returns overhang values of the specified NGInlineItemResult representing
// LayoutNGRubyRun.
//
// This is used by NGLineBreaker.
NGAnnotationOverhang GetOverhang(const NGInlineItemResult& item);

// Returns true if |start_overhang| is applied to a previous item, and
// clamp |start_overhang| to the width of the previous item.
//
// This is used by NGLineBreaker.
bool CanApplyStartOverhang(const NGLineInfo& line_info,
                           LayoutUnit& start_overhang);

// This should be called after NGInlineItemResult for a text is added in
// NGLineBreaker::HandleText().
//
// This function may update a NGInlineItemResult representing RubyRun
// in |line_info|
LayoutUnit CommitPendingEndOverhang(NGLineInfo* line_info);

// Stores ComputeAnnotationOverflow() results.
//
// |overflow_over| and |space_over| are exclusive. Only one of them can be
// non-zero. |overflow_under| and |space_under| are exclusive too.
// All fields never be negative.
struct NGAnnotationMetrics {
  // The amount of annotation overflow at the line-over side.
  LayoutUnit overflow_over;
  // The amount of annotation overflow at the line-under side.
  LayoutUnit overflow_under;
  // The amount of annotation space which the next line at the line-over
  // side can consume.
  LayoutUnit space_over;
  // The amount of annotation space which the next line at the line-under
  // side can consume.
  LayoutUnit space_under;
};

// Compute over/under annotation overflow/space for the specified line.
NGAnnotationMetrics ComputeAnnotationOverflow(
    const NGLogicalLineItems& logical_line,
    const NGLineHeightMetrics& line_box_metrics,
    LayoutUnit line_over,
    const ComputedStyle& line_style);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_RUBY_UTILS_H_
