// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_RANGE_H_
#define UI_ACCESSIBILITY_AX_RANGE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

// A range of ax positions.
//
// In order to avoid any confusion regarding whether a deep or a shallow copy is
// being performed, this class can be moved but not copied.
template <class AXPositionType>
class AXRange {
 public:
  using AXPositionInstance = std::unique_ptr<AXPositionType>;

  AXRange()
      : anchor_(AXPositionType::CreateNullPosition()),
        focus_(AXPositionType::CreateNullPosition()) {}

  AXRange(AXPositionInstance anchor, AXPositionInstance focus) {
    if (anchor) {
      anchor_ = std::move(anchor);
    } else {
      anchor_ = AXPositionType::CreateNullPosition();
    }
    if (focus) {
      focus_ = std::move(focus);
    } else {
      focus = AXPositionType::CreateNullPosition();
    }
  }

  AXRange(const AXRange& other) = delete;

  AXRange(AXRange&& other) : AXRange() {
    anchor_.swap(other.anchor_);
    focus_.swap(other.focus_);
  }

  AXRange& operator=(const AXRange& other) = delete;

  AXRange& operator=(const AXRange&& other) {
    if (this != other) {
      anchor_ = AXPositionType::CreateNullPosition();
      focus_ = AXPositionType::CreateNullPosition();
      anchor_.swap(other.anchor_);
      focus_.swap(other.focus_);
    }
    return *this;
  }

  bool operator==(const AXRange& other) const {
    if (IsNull())
      return other.IsNull();
    return !other.IsNull() && *anchor_ == *other.anchor() &&
           *focus_ == *other.focus();
  }

  bool operator!=(const AXRange& other) const { return !(*this == other); }

  virtual ~AXRange() {}

  bool IsNull() const {
    return !anchor_ || !focus_ || anchor_->IsNullPosition() ||
           focus_->IsNullPosition();
  }

  AXPositionType* anchor() const {
    DCHECK(anchor_);
    return anchor_.get();
  }

  AXPositionType* focus() const {
    DCHECK(focus_);
    return focus_.get();
  }

  AXRange GetForwardRange() const {
    DCHECK(!IsNull());
    if (*focus_ < *anchor_)
      return AXRange(focus_->Clone(), anchor_->Clone());
    return AXRange(anchor_->Clone(), focus_->Clone());
  }

  base::string16 GetText() const {
    if (IsNull() || *anchor_ == *focus_)
      return base::string16();

    AXRange forward_range = GetForwardRange();
    AXPositionInstance start = forward_range.anchor()->AsLeafTextPosition();
    AXPositionInstance end = forward_range.focus()->AsLeafTextPosition();

    int start_offset = start->text_offset();
    DCHECK_GE(start_offset, 0);
    int end_offset = end->text_offset();
    DCHECK_GE(end_offset, 0);

    base::string16 text;
    do {
      text += start->GetText();
      start = start->CreateNextTextAnchorPosition();
    } while (!start->IsNullPosition() && *start < *end);

    if (static_cast<size_t>(start_offset) > text.length())
      return base::string16();

    text = text.substr(start_offset, base::string16::npos);

    // If the loop above traversed the entire range, don't trim
    if (*start == *end && !start->IsNullPosition())
      return text;

    size_t text_length = text.length() - end->GetText().length() +
                         static_cast<size_t>(end_offset);
    return text.substr(0, text_length);
  }

  // Returns a vector of all ranges (each of them spanning a single anchor)
  // between the anchor_ and focus_ endpoints of this instance.
  std::vector<AXRange<AXPositionType>> GetAnchors() const {
    std::vector<AXRange<AXPositionType>> anchors;
    if (IsNull())
      return anchors;

    AXRange forward_range = GetForwardRange();
    AXPositionInstance current_anchor_start =
        forward_range.anchor()->AsLeafTextPosition();
    AXPositionInstance range_end = forward_range.focus()->AsLeafTextPosition();

    while (!current_anchor_start->IsNullPosition()) {
      // When the current start reaches the same anchor as this AXRange's end,
      // simply append this last anchor (trimmed at range_end) and exit.
      if (current_anchor_start->GetAnchor() == range_end->GetAnchor()) {
        anchors.emplace_back(current_anchor_start->Clone(), range_end->Clone());
        return anchors;
      }

      AXPositionInstance current_anchor_end =
          current_anchor_start->CreatePositionAtEndOfAnchor();
      DCHECK_EQ(current_anchor_start->GetAnchor(),
                current_anchor_end->GetAnchor());
      DCHECK_LE(*current_anchor_start, *current_anchor_end);
      DCHECK_LE(*current_anchor_end, *range_end);

      anchors.emplace_back(current_anchor_start->Clone(),
                           current_anchor_end->Clone());
      current_anchor_start =
          current_anchor_end->CreateNextAnchorPosition()->AsLeafTextPosition();
    }
    return anchors;
  }

  // Appends rects in screen coordinates of all anchor nodes that span between
  // anchor_ and focus_. Rects outside of the viewport are skipped.
  std::vector<gfx::Rect> GetScreenRects() const {
    std::vector<gfx::Rect> rects;

    for (const AXRange& anchor_range : GetAnchors()) {
      DCHECK(!anchor_range.IsNull());
      AXPositionInstance anchor_end =
          anchor_range.focus()->AsLeafTextPosition();
      AXPositionInstance current_line_start =
          anchor_range.anchor()->AsLeafTextPosition();

      while (current_line_start->GetAnchor() == anchor_end->GetAnchor() &&
             *current_line_start <= *anchor_end) {
        AXPositionInstance current_line_end =
            current_line_start->CreateNextLineEndPosition(
                ui::AXBoundaryBehavior::CrossBoundary);

        if (current_line_end->GetAnchor() != anchor_end->GetAnchor() ||
            *current_line_end > *anchor_end)
          current_line_end = anchor_end->Clone();

        DCHECK_LE(*current_line_start, *current_line_end);
        DCHECK_LE(*current_line_end, *anchor_end);

        if (current_line_start->GetAnchor()->data().role ==
            ax::mojom::Role::kInlineTextBox) {
          current_line_start = current_line_start->CreateParentPosition();
          current_line_end = current_line_end->CreateParentPosition();
        }

        AXTreeID current_tree_id = current_line_start->tree_id();
        AXTreeManager* manager =
            AXTreeManagerMap::GetInstance().GetManager(current_tree_id);
        AXNode* current_anchor = current_line_start->GetAnchor();
        AXPlatformNodeDelegate* current_anchor_delegate =
            manager->GetDelegate(current_tree_id, current_anchor->id());

        // For text anchors, we retrieve the bounding rectangles of its text
        // content. For non-text anchors (such as checkboxes, images, etc.), we
        // want to directly retrieve their bounding rectangles.
        gfx::Rect current_rect;
        if (IsTextOrLineBreak(current_anchor->data().role))
          current_rect = current_anchor_delegate->GetInnerTextRangeBoundsRect(
              current_line_start->text_offset(),
              current_line_end->text_offset(), AXCoordinateSystem::kScreen,
              AXClippingBehavior::kClipped);
        else
          current_rect = current_anchor_delegate->GetBoundsRect(
              AXCoordinateSystem::kScreen, AXClippingBehavior::kClipped);

        // We only add rects that are visible within the current viewport.
        // If the bounding rectangle is outside the viewport, the `kClipped`
        // parameter from the bounds APIs will result in returning an empty
        // rect, which we should omit from the final result.
        if (!current_rect.IsEmpty())
          rects.push_back(current_rect);

        current_line_start = current_line_end->CreateNextLineStartPosition(
            ui::AXBoundaryBehavior::CrossBoundary);
      }
    }
    return rects;
  }

 private:
  AXPositionInstance anchor_;
  AXPositionInstance focus_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_RANGE_H_
