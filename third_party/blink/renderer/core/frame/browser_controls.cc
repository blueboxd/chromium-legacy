// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/browser_controls.h"

#include <algorithm>  // for std::min and std::max

#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

BrowserControls::BrowserControls(const Page& page)
    : page_(&page),
      top_shown_ratio_(0),
      bottom_shown_ratio_(0),
      baseline_top_content_offset_(0),
      baseline_bottom_content_offset_(0),
      accumulated_scroll_delta_(0),
      permitted_state_(cc::BrowserControlsState::kBoth) {}

void BrowserControls::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
}

void BrowserControls::ScrollBegin() {
  ResetBaseline();
}

FloatSize BrowserControls::ScrollBy(FloatSize pending_delta) {
  // If one or both of the top/bottom controls are showing, the shown ratio
  // needs to be computed.
  if (!TopHeight() && !BottomHeight())
    return pending_delta;

  if ((permitted_state_ == cc::BrowserControlsState::kShown &&
       pending_delta.Height() > 0) ||
      (permitted_state_ == cc::BrowserControlsState::kHidden &&
       pending_delta.Height() < 0))
    return pending_delta;

  float page_scale = page_->GetVisualViewport().Scale();

  // Update accumulated vertical scroll and apply it to browser controls
  // Compute scroll delta in viewport space by applying page scale
  accumulated_scroll_delta_ += pending_delta.Height() * page_scale;

  // We want to base our calculations on top or bottom controls. After consuming
  // the scroll delta, we will calculate a shown ratio for the controls. The
  // top controls have the priority because they need to visually be in sync
  // with the web contents.
  bool base_on_top_controls = TopHeight();

  float old_top_offset = ContentOffset();
  float baseline_content_offset = base_on_top_controls
                                      ? baseline_top_content_offset_
                                      : baseline_bottom_content_offset_;
  float new_content_offset =
      baseline_content_offset - accumulated_scroll_delta_;
  float height = base_on_top_controls ? TopHeight() : BottomHeight();
  // Clamp and use the expected content offset so that we don't return
  // spurious remaining scrolls due to the imprecision of the shownRatio.
  new_content_offset = clampTo(
      new_content_offset,
      base_on_top_controls ? TopMinHeight() : BottomMinHeight(), height);

  // The top and bottom controls ratios can be calculated independently.
  // However, we want the (normalized) ratios to be equal when scrolling.
  float shown_ratio = new_content_offset / height;
  float min_ratio =
      base_on_top_controls ? TopMinShownRatio() : BottomMinShownRatio();
  float normalized_shown_ratio =
      (clampTo(shown_ratio, min_ratio, 1.f) - min_ratio) / (1.f - min_ratio);
  // Even though the real shown ratios (shown height / total height) of the top
  // and bottom controls can be different, they share the same
  // relative/normalized ratio to keep them in sync.
  SetShownRatio(
      TopMinShownRatio() + normalized_shown_ratio * (1.f - TopMinShownRatio()),
      BottomMinShownRatio() +
          normalized_shown_ratio * (1.f - BottomMinShownRatio()));

  // Reset baseline when controls are fully visible
  if (top_shown_ratio_ == 1 && bottom_shown_ratio_ == 1)
    ResetBaseline();

  // We negate the difference because scrolling down (positive delta) causes
  // browser controls to hide (negative offset difference).
  FloatSize applied_delta(
      0, base_on_top_controls
             ? (old_top_offset - new_content_offset) / page_scale
             : 0);
  return pending_delta - applied_delta;
}

void BrowserControls::ResetBaseline() {
  accumulated_scroll_delta_ = 0;
  baseline_top_content_offset_ = ContentOffset();
  baseline_bottom_content_offset_ = BottomContentOffset();
}

float BrowserControls::UnreportedSizeAdjustment() {
  return (ShrinkViewport() ? TopHeight() : 0) - ContentOffset();
}

float BrowserControls::ContentOffset() {
  return top_shown_ratio_ * TopHeight();
}

// Even though this is called *ContentOffset, the value from here isn't used to
// offset the content because only the top controls should do that. For now, the
// BottomContentOffset is the baseline offset when we don't have top controls.
float BrowserControls::BottomContentOffset() {
  return bottom_shown_ratio_ * BottomHeight();
}

void BrowserControls::SetShownRatio(float top_ratio, float bottom_ratio) {
  top_ratio = clampTo(top_ratio, 0.f, 1.f);
  bottom_ratio = clampTo(bottom_ratio, 0.f, 1.f);

  if (top_shown_ratio_ == top_ratio && bottom_shown_ratio_ == bottom_ratio)
    return;

  top_shown_ratio_ = top_ratio;
  bottom_shown_ratio_ = bottom_ratio;
  page_->GetChromeClient().DidUpdateBrowserControls();
}

void BrowserControls::UpdateConstraintsAndState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate) {
  permitted_state_ = constraints;

  DCHECK(!(constraints == cc::BrowserControlsState::kShown &&
           current == cc::BrowserControlsState::kHidden));
  DCHECK(!(constraints == cc::BrowserControlsState::kHidden &&
           current == cc::BrowserControlsState::kShown));
}

void BrowserControls::SetParams(cc::BrowserControlsParams params) {
  if (params_ == params) {
    return;
  }

  params_ = params;
  page_->GetChromeClient().DidUpdateBrowserControls();
}

float BrowserControls::TopMinShownRatio() {
  return TopHeight() ? params_.top_controls_min_height / TopHeight() : 0.f;
}

float BrowserControls::BottomMinShownRatio() {
  return BottomHeight() ? params_.bottom_controls_min_height / BottomHeight()
                        : 0.f;
}

}  // namespace blink
