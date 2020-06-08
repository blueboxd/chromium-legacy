// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_RUBY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_RUBY_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class NGLineInfo;
struct NGInlineItemResult;

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_RUBY_UTILS_H_
