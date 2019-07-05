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

// A range delimited by two positions in the AXTree.
//
// In order to avoid any confusion regarding whether a deep or a shallow copy is
// being performed, this class can be moved, but not copied.
template <class AXPositionType>
class AXRange {
 public:
  using AXPositionInstance = std::unique_ptr<AXPositionType>;

  AXRange()
      : anchor_(AXPositionType::CreateNullPosition()),
        focus_(AXPositionType::CreateNullPosition()) {}

  AXRange(AXPositionInstance anchor, AXPositionInstance focus) {
    anchor_ = anchor ? std::move(anchor) : AXPositionType::CreateNullPosition();
    focus_ = focus ? std::move(focus) : AXPositionType::CreateNullPosition();
  }

  AXRange(const AXRange& other) = delete;

  AXRange(AXRange&& other) : AXRange() {
    anchor_.swap(other.anchor_);
    focus_.swap(other.focus_);
  }

  virtual ~AXRange() {}

  AXPositionType* anchor() const {
    DCHECK(anchor_);
    return anchor_.get();
  }

  AXPositionType* focus() const {
    DCHECK(focus_);
    return focus_.get();
  }

  bool IsNull() const {
    DCHECK(anchor_ && focus_);
    return anchor_->IsNullPosition() || focus_->IsNullPosition();
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

  AXRange GetForwardRange() const {
    return (*focus_ < *anchor_) ? AXRange(focus_->Clone(), anchor_->Clone())
                                : AXRange(anchor_->Clone(), focus_->Clone());
  }

  // We define a "leaf text range" as an AXRange whose endpoints are leaf text
  // positions located within the same anchor of the AXTree.
  bool IsLeafTextRange() const {
    return !IsNull() && anchor_->GetAnchor() == focus_->GetAnchor() &&
           anchor_->IsLeafTextPosition() && focus_->IsLeafTextPosition();
  }

  // We can decompose any given AXRange into multiple "leaf text ranges".
  // As an example, consider the following HTML code:
  //
  //   <p>line with text<br><input type="checkbox">line with checkbox</p>
  //
  // It will produce the following AXTree; notice that the leaf text nodes
  // (enclosed in parenthesis) compose its text representation:
  //
  //   paragraph
  //     staticText name='line with text'
  //       (inlineTextBox name='line with text')
  //     lineBreak name='<newline>'
  //       (inlineTextBox name='<newline>')
  //     (checkBox)
  //     staticText name='line with checkbox'
  //       (inlineTextBox name='line with checkbox')
  //
  // Suppose we have an AXRange containing all elements from the example above.
  // The text representation of such range, with AXRange's endpoints marked by
  // opening and closing brackets, will look like the following:
  //
  //   "[line with text\n{checkBox}line with checkbox]"
  //
  // Note that in the text representation {checkBox} is not visible, but it is
  // effectively a "leaf text range", so we include it in the example above only
  // to visualize how the iterator should work.
  //
  // Decomposing the AXRange above into its "leaf text ranges" would result in:
  //
  //   "[line with text][\n][{checkBox}][line with checkbox]"
  //
  // This class allows AXRange to be iterated through all "leaf text ranges"
  // contained between its endpoints, composing the entire range.
  class Iterator : public std::iterator<std::input_iterator_tag, AXRange> {
   public:
    Iterator()
        : current_start_(AXPositionType::CreateNullPosition()),
          iterator_end_(AXPositionType::CreateNullPosition()) {}

    Iterator(AXPositionInstance start, AXPositionInstance end) {
      if (end && !end->IsNullPosition()) {
        current_start_ = !start ? AXPositionType::CreateNullPosition()
                                : start->AsLeafTextPosition();
        iterator_end_ = end->AsLeafTextPosition();
      } else {
        current_start_ = AXPositionType::CreateNullPosition();
        iterator_end_ = AXPositionType::CreateNullPosition();
      }
    }

    Iterator(const Iterator& other) = delete;

    Iterator(Iterator&& other)
        : current_start_(std::move(other.current_start_)),
          iterator_end_(std::move(other.iterator_end_)) {}

    ~Iterator() = default;

    bool operator==(const Iterator& other) const {
      return current_start_->GetAnchor() == other.current_start_->GetAnchor() &&
             iterator_end_->GetAnchor() == other.iterator_end_->GetAnchor() &&
             *current_start_ == *other.current_start_ &&
             *iterator_end_ == *other.iterator_end_;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    // Only forward iteration is supported, so operator-- is not implemented.
    Iterator& operator++() {
      DCHECK(!current_start_->IsNullPosition());
      if (current_start_->GetAnchor() == iterator_end_->GetAnchor()) {
        current_start_ = AXPositionType::CreateNullPosition();
      } else {
        current_start_ =
            current_start_->CreateNextAnchorPosition()->AsLeafTextPosition();
        DCHECK_LE(*current_start_, *iterator_end_);
      }
      return *this;
    }

    AXRange operator*() const {
      DCHECK(!current_start_->IsNullPosition());
      AXPositionInstance current_end =
          (current_start_->GetAnchor() != iterator_end_->GetAnchor())
              ? current_start_->CreatePositionAtEndOfAnchor()
              : iterator_end_->Clone();
      DCHECK_LE(*current_end, *iterator_end_);

      AXRange current_anchor_range(current_start_->Clone(),
                                   std::move(current_end));
      DCHECK(current_anchor_range.IsLeafTextRange());
      return std::move(current_anchor_range);
    }

   private:
    AXPositionInstance current_start_;
    AXPositionInstance iterator_end_;
  };

  Iterator begin() const {
    AXRange forward_range = GetForwardRange();
    return Iterator(std::move(forward_range.anchor_),
                    std::move(forward_range.focus_));
  }

  Iterator end() const {
    AXRange forward_range = GetForwardRange();
    return Iterator(nullptr, std::move(forward_range.focus_));
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

  // Appends rects in screen coordinates of all anchor nodes that span between
  // anchor_ and focus_. Rects outside of the viewport are skipped.
  std::vector<gfx::Rect> GetScreenRects() const {
    std::vector<gfx::Rect> rects;

    for (const AXRange& anchor_range : *this) {
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
