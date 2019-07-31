// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_POSITION_H_
#define UI_ACCESSIBILITY_AX_POSITION_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/stack.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_text_styles.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

// Defines the type of position in the accessibility tree.
// A tree position is used when referring to a specific child of a node in the
// accessibility tree.
// A text position is used when referring to a specific character of text inside
// a particular node.
// A null position is used to signify that the provided data is invalid or that
// a boundary has been reached.
enum class AXPositionKind { NULL_POSITION, TREE_POSITION, TEXT_POSITION };

// Defines how creating the next or previous position should behave whenever we
// are at or are crossing a boundary, such as at the start of an anchor, a word
// or a line.
enum class AXBoundaryBehavior {
  CrossBoundary,
  StopAtAnchorBoundary,
  StopIfAlreadyAtBoundary
};

// Forward declarations.
template <class AXPositionType, class AXNodeType>
class AXPosition;
template <class AXPositionType, class AXNodeType>
bool operator==(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second);
template <class AXPositionType, class AXNodeType>
bool operator!=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second);

// A position in the accessibility tree.
//
// This class could either represent a tree position or a text position.
// Tree positions point to either a child of a specific node or at the end of a
// node (i.e. an "after children" position).
// Text positions point to either a character offset in the text inside a
// particular node including text from all its children, or to the end of the
// node's text, (i.e. an "after text" position).
// On tree positions that have a leaf node as their anchor, we also need to
// distinguish between "before text" and "after text" positions. To do this, if
// the child index is 0 and the anchor is a leaf node, then it's an "after text"
// position. If the child index is |BEFORE_TEXT| and the anchor is a leaf node,
// then this is a "before text" position.
// It doesn't make sense to have a "before text" position on a text position,
// because it is identical to setting its offset to the first character.
//
// To avoid re-computing either the text offset or the child index when
// converting between the two types of positions, both values are saved after
// the first conversion.
//
// This class template uses static polymorphism in order to allow sub-classes to
// be created from the base class without the base class knowing the type of the
// sub-class in advance.
// The template argument |AXPositionType| should always be set to the type of
// any class that inherits from this template, making this a
// "curiously recursive template".
//
// This class can be copied using the |Clone| method. It is designed to be
// immutable.

template <class AXPositionType, class AXNodeType>
class AXPosition {
 public:
  using AXPositionInstance =
      std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>;

  static const int32_t INVALID_ANCHOR_ID = -1;
  static const int BEFORE_TEXT = -1;
  static const int INVALID_INDEX = -2;
  static const int INVALID_OFFSET = -1;

  static AXPositionInstance CreateNullPosition() {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::NULL_POSITION, AXTreeIDUnknown(),
                             INVALID_ANCHOR_ID, INVALID_INDEX, INVALID_OFFSET,
                             ax::mojom::TextAffinity::kDownstream);
    return new_position;
  }

  static AXPositionInstance CreateTreePosition(AXTreeID tree_id,
                                               int32_t anchor_id,
                                               int child_index) {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::TREE_POSITION, tree_id, anchor_id,
                             child_index, INVALID_OFFSET,
                             ax::mojom::TextAffinity::kDownstream);
    return new_position;
  }

  static AXPositionInstance CreateTextPosition(
      AXTreeID tree_id,
      int32_t anchor_id,
      int text_offset,
      ax::mojom::TextAffinity affinity) {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::TEXT_POSITION, tree_id, anchor_id,
                             INVALID_INDEX, text_offset, affinity);
    return new_position;
  }

  virtual ~AXPosition() = default;

  virtual AXPositionInstance Clone() const = 0;

  std::string ToString() const {
    std::string str;
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return "NullPosition";
      case AXPositionKind::TREE_POSITION: {
        std::string str_child_index;
        if (child_index_ == BEFORE_TEXT) {
          str_child_index = "before_text";
        } else if (child_index_ == INVALID_INDEX) {
          str_child_index = "invalid";
        } else {
          str_child_index = base::NumberToString(child_index_);
        }
        str = "TreePosition tree_id=" + tree_id_.ToString() +
              " anchor_id=" + base::NumberToString(anchor_id_) +
              " child_index=" + str_child_index;
        break;
      }
      case AXPositionKind::TEXT_POSITION: {
        std::string str_text_offset;
        if (text_offset_ == INVALID_OFFSET) {
          str_text_offset = "invalid";
        } else {
          str_text_offset = base::NumberToString(text_offset_);
        }
        str = "TextPosition anchor_id=" + base::NumberToString(anchor_id_) +
              " text_offset=" + str_text_offset + " affinity=" +
              ui::ToString(static_cast<ax::mojom::TextAffinity>(affinity_));
        break;
      }
    }

    if (!IsTextPosition() || text_offset_ > MaxTextOffset())
      return str;

    std::string text = base::UTF16ToUTF8(GetText());
    DCHECK_GE(text_offset_, 0);
    DCHECK_LE(text_offset_, static_cast<int>(text.length()));
    std::string annotated_text;
    if (text_offset_ == MaxTextOffset()) {
      annotated_text = text + "<>";
    } else {
      annotated_text = text.substr(0, text_offset_) + "<" + text[text_offset_] +
                       ">" + text.substr(text_offset_ + 1);
    }

    return str + " annotated_text=" + annotated_text;
  }

  AXTreeID tree_id() const { return tree_id_; }
  int32_t anchor_id() const { return anchor_id_; }

  AXNodeType* GetAnchor() const {
    if (tree_id_ == AXTreeIDUnknown() || anchor_id_ == INVALID_ANCHOR_ID)
      return nullptr;
    DCHECK_GE(anchor_id_, 0);
    return GetNodeInTree(tree_id_, anchor_id_);
  }

  AXPositionKind kind() const { return kind_; }
  int child_index() const { return child_index_; }
  int text_offset() const { return text_offset_; }
  ax::mojom::TextAffinity affinity() const { return affinity_; }

  bool IsNullPosition() const {
    return kind_ == AXPositionKind::NULL_POSITION || !GetAnchor();
  }

  bool IsTreePosition() const {
    return GetAnchor() && kind_ == AXPositionKind::TREE_POSITION;
  }

  bool IsTextPosition() const {
    return GetAnchor() && kind_ == AXPositionKind::TEXT_POSITION;
  }

  bool IsLeafTextPosition() const {
    return IsTextPosition() && !AnchorChildCount();
  }

  // TODO(nektar): Update logic of AtStartOfAnchor() for text_offset_ == 0 and
  // fix related bug.
  bool AtStartOfAnchor() const {
    if (!GetAnchor())
      return false;

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        if (text_offset_ > 0)
          return false;
        if (AnchorChildCount() || text_offset_ == 0)
          return child_index_ == 0;
        return child_index_ == BEFORE_TEXT;
      case AXPositionKind::TEXT_POSITION:
        return text_offset_ == 0;
    }

    return false;
  }

  bool AtEndOfAnchor() const {
    if (!GetAnchor())
      return false;

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        return child_index_ == AnchorChildCount();
      case AXPositionKind::TEXT_POSITION:
        return text_offset_ == MaxTextOffset();
    }

    return false;
  }

  bool AtStartOfWord() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        const std::vector<int32_t> word_starts =
            text_position->GetWordStartOffsets();
        return base::Contains(
            word_starts, static_cast<int32_t>(text_position->text_offset_));
      }
    }
    return false;
  }

  bool AtEndOfWord() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        const std::vector<int32_t> word_ends =
            text_position->GetWordEndOffsets();
        return base::Contains(
            word_ends, static_cast<int32_t>(text_position->text_offset_));
      }
    }
    return false;
  }

  bool AtStartOfLine() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION:
        // We treat a position after some white space that is not connected to
        // any node after it via "next on line ID", to be equivalent to a
        // position before the next line, and therefore as being at start of
        // line.
        //
        // We assume that white space separates lines.
        if (text_position->IsInWhiteSpace() &&
            GetNextOnLineID(text_position->anchor_id_) == INVALID_ANCHOR_ID &&
            text_position->AtEndOfAnchor()) {
          return true;
        }

        return GetPreviousOnLineID(text_position->anchor_id_) ==
                   INVALID_ANCHOR_ID &&
               text_position->AtStartOfAnchor();
    }
    return false;
  }

  bool AtEndOfLine() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION:
        // Text positions on objects with no text should not be considered at
        // end of line because the empty position may share a text offset with
        // a non-empty text position in which case the end of line iterators
        // must move to the line end of the non-empty content. Specified next
        // line IDs are ignored.
        if (!text_position->MaxTextOffset()) {
          return false;
        }

        // If affinity has been used to specify whether the caret is at the end
        // of a line or at the start of the next one, this should have been
        // reflected in the leaf text position we got. In other cases, we
        // assume that white space is being used to separate lines.
        //
        // We don't treat a position that is at the start of white space that is
        // on a line by itself as being at the end of the line. However, we do
        // treat positions at the start of white space that end a line of text
        // as being at the end of that line. We also treat positions at the end
        // of white space that is on a line by itself as being at the end of
        // that line. Note that white space that ends a line of text should be
        // connected to that text with a "previous on line ID".
        if (GetNextOnLineID(text_position->anchor_id_) == INVALID_ANCHOR_ID) {
          if (text_position->IsInWhiteSpace()) {
            if (GetPreviousOnLineID(text_position->anchor_id_) ==
                INVALID_ANCHOR_ID) {
              return text_position->AtEndOfAnchor();
            } else {
              return text_position->AtStartOfAnchor();
            }
          }

          return text_position->AtEndOfAnchor();
        }

        // The current anchor might be followed by a soft line break.
        if (text_position->AtEndOfAnchor())
          return text_position->CreateNextTextAnchorPosition()->AtEndOfLine();
    }
    return false;
  }

  bool AtStartOfParagraph() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        if (!text_position->AtStartOfAnchor())
          return false;

        // Search for the previous text position within the current paragraph,
        // using the paragraph boundary abort predicate.
        // If a valid position was found, then this position cannot be
        // the start of a paragraph.
        // This will return a null position when an anchor movement would
        // cross a paragraph boundary, or the start of document was reached.
        auto previous_text_position =
            text_position->CreatePreviousTextAnchorPosition(
                base::BindRepeating(&AbortMoveAtParagraphBoundary));
        return previous_text_position->IsNullPosition();
      }
    }
    return false;
  }

  bool AtEndOfParagraph() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        if (!text_position->AtEndOfAnchor())
          return false;

        // Search for the next text position within the current paragraph,
        // using the paragraph boundary abort predicate.
        // If a valid position was found, then this position cannot be
        // the end of a paragraph.
        // This will return a null position when an anchor movement would
        // cross a paragraph boundary, or the end of document was reached.
        auto next_text_position = text_position->CreateNextTextAnchorPosition(
            base::BindRepeating(&AbortMoveAtParagraphBoundary));
        return next_text_position->IsNullPosition();
      }
    }
    return false;
  }

  bool AtStartOfDocument() const {
    if (IsNullPosition())
      return false;
    return IsDocument(GetRole()) && AtStartOfAnchor();
  }

  bool AtEndOfDocument() const {
    if (IsNullPosition())
      return false;
    return CreateNextAnchorPosition()->IsNullPosition() && AtEndOfAnchor();
  }

  // This method returns a position instead of a node because this allows us to
  // return the corresponding text offset or child index in the ancestor that
  // relates to the current position.
  // Also, this method uses position instead of tree logic to traverse the tree,
  // because positions can handle moving across multiple trees, while trees
  // cannot.
  AXPositionInstance LowestCommonAncestor(
      const AXPosition<AXPositionType, AXNodeType>& second) const {
    if (IsNullPosition() || second.IsNullPosition())
      return CreateNullPosition();
    if (GetAnchor() == second.GetAnchor())
      return Clone();

    base::stack<AXNodeType*> our_ancestors = GetAncestorAnchors();
    base::stack<AXNodeType*> other_ancestors = second.GetAncestorAnchors();

    AXNodeType* common_anchor = nullptr;
    while (!our_ancestors.empty() && !other_ancestors.empty() &&
           our_ancestors.top() == other_ancestors.top()) {
      common_anchor = our_ancestors.top();
      our_ancestors.pop();
      other_ancestors.pop();
    }
    if (!common_anchor)
      return CreateNullPosition();

    AXPositionInstance common_ancestor = Clone();
    while (!common_ancestor->IsNullPosition() &&
           common_ancestor->GetAnchor() != common_anchor) {
      common_ancestor = common_ancestor->CreateParentPosition();
    }
    return common_ancestor;
  }

  AXPositionInstance AsTreePosition() const {
    if (IsNullPosition() || IsTreePosition())
      return Clone();

    AXPositionInstance copy = Clone();
    DCHECK(copy);
    DCHECK_GE(copy->text_offset_, 0);
    if (!copy->AnchorChildCount() &&
        copy->text_offset_ != copy->MaxTextOffset()) {
      copy->child_index_ = BEFORE_TEXT;
    } else {
      copy->child_index_ = 0;
    }

    // Blink doesn't always remove all deleted whitespace at the end of a
    // textarea even though it will have adjusted its value attribute, because
    // the extra layout objects are invisible. Therefore, we will stop at the
    // last child that we can reach with the current text offset and ignore any
    // remaining children.
    int current_offset = 0;
    for (int i = 0; i < copy->AnchorChildCount(); ++i) {
      AXPositionInstance child = copy->CreateChildPositionAt(i);
      DCHECK(child);
      int child_length = child->MaxTextOffsetInParent();
      if (copy->text_offset_ >= current_offset &&
          (copy->text_offset_ < (current_offset + child_length) ||
           (copy->affinity_ == ax::mojom::TextAffinity::kUpstream &&
            copy->text_offset_ == (current_offset + child_length)))) {
        copy->child_index_ = i;
        break;
      }

      current_offset += child_length;
    }
    if (current_offset >= copy->MaxTextOffset())
      copy->child_index_ = copy->AnchorChildCount();

    copy->kind_ = AXPositionKind::TREE_POSITION;
    return copy;
  }

  AXPositionInstance AsTextPosition() const {
    if (IsNullPosition() || IsTextPosition())
      return Clone();

    AXPositionInstance copy = Clone();
    DCHECK(copy);
    // Check if it is a "before text" position.
    if (copy->child_index_ == BEFORE_TEXT) {
      // "Before text" positions can only appear on leaf nodes.
      DCHECK(!copy->AnchorChildCount());
      // If the current text offset is valid, we don't touch it to potentially
      // allow converting from a text position to a tree position and back
      // without losing information.
      if (copy->text_offset_ < 0 || copy->text_offset_ >= copy->MaxTextOffset())
        copy->text_offset_ = 0;
    } else if (copy->child_index_ == copy->AnchorChildCount()) {
      copy->text_offset_ = copy->MaxTextOffset();
    } else {
      DCHECK_GE(copy->child_index_, 0);
      DCHECK_LT(copy->child_index_, copy->AnchorChildCount());
      int new_offset = 0;
      for (int i = 0; i <= child_index_; ++i) {
        AXPositionInstance child = copy->CreateChildPositionAt(i);
        DCHECK(child);
        int child_length = child->MaxTextOffsetInParent();

        // If the current text offset is valid, we don't touch it to potentially
        // allow converting from a text position to a tree position and back
        // without losing information. Otherwise, we reset it to the beginning
        // of the current child node.
        if (i == child_index_ &&
            (copy->text_offset_ < new_offset ||
             copy->text_offset_ > (new_offset + child_length) ||
             // When the text offset is equal to the text's length but this is
             // not an "after text" position.
             (!copy->AtEndOfAnchor() &&
              copy->text_offset_ == (new_offset + child_length)))) {
          copy->text_offset_ = new_offset;
          break;
        }

        new_offset += child_length;
      }
    }

    // Affinity should always be left as downstream. The only case when the
    // resulting text position is at the end of the line is when we get an
    // "after text" leaf position, but even in this case downstream is
    // appropriate because there is no ambiguity whetehr the position is at the
    // end of the current line vs. the start of the next line. It would always
    // be the former.
    copy->kind_ = AXPositionKind::TEXT_POSITION;
    return copy;
  }

  AXPositionInstance AsLeafTextPosition() const {
    if (IsNullPosition() || !AnchorChildCount())
      return AsTextPosition();

    AXPositionInstance tree_position = AsTreePosition();
    // Adjust the text offset.
    // No need to check for "before text" positions here because they are only
    // present on leaf anchor nodes.
    int adjusted_offset = AsTextPosition()->text_offset_;
    AXPositionInstance child_position = tree_position->CreateChildPositionAt(0);
    DCHECK(child_position);
    for (int i = 1;
         i <= tree_position->child_index_ &&
         i < tree_position->AnchorChildCount() && adjusted_offset > 0;
         ++i) {
      adjusted_offset -= child_position->MaxTextOffsetInParent();
      child_position = tree_position->CreateChildPositionAt(i);
      DCHECK(child_position);
    }

    child_position = child_position->AsTextPosition();
    child_position->text_offset_ = adjusted_offset;
    // Maintain affinity from parent so that we'll be able to choose the correct
    // leaf anchor if the text offset is right on the boundary between two
    // leaves.
    child_position->affinity_ = affinity_;
    return child_position->AsLeafTextPosition();
  }

  AXPositionInstance CreatePositionAtStartOfAnchor() const {
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        if (!AnchorChildCount()) {
          return CreateTreePosition(tree_id_, anchor_id_, BEFORE_TEXT);
        }
        return CreateTreePosition(tree_id_, anchor_id_, 0 /* child_index */);
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(tree_id_, anchor_id_, 0 /* text_offset */,
                                  ax::mojom::TextAffinity::kDownstream);
    }
    return CreateNullPosition();
  }

  AXPositionInstance CreatePositionAtEndOfAnchor() const {
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        return CreateTreePosition(tree_id_, anchor_id_, AnchorChildCount());
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(tree_id_, anchor_id_, MaxTextOffset(),
                                  ax::mojom::TextAffinity::kDownstream);
    }
    return CreateNullPosition();
  }

  AXPositionInstance CreatePreviousFormatStartPosition(
      ui::AXBoundaryBehavior boundary_behavior) const {
    return CreatePositionAtFormatBoundary(boundary_behavior,
                                          /*forwards*/ false);
  }

  AXPositionInstance CreateNextFormatEndPosition(
      ui::AXBoundaryBehavior boundary_behavior) const {
    return CreatePositionAtFormatBoundary(boundary_behavior, /*forwards*/ true);
  }

  AXPositionInstance CreatePositionAtFormatBoundary(
      AXBoundaryBehavior boundary_behavior,
      bool forwards) const {
    // Disallow AXBoundaryBehavior::StopAtAnchorBoundary, as it would be no
    // different than moving by anchor
    DCHECK_NE(boundary_behavior, AXBoundaryBehavior::StopAtAnchorBoundary);

    if (IsNullPosition())
      return CreateNullPosition();

    bool was_tree_position = IsTreePosition();
    AXPositionInstance initial_endpoint = AsLeafTextPosition();
    AXPositionInstance current_endpoint =
        forwards ? initial_endpoint->CreateNextTextAnchorPosition()
                 : initial_endpoint->CreatePreviousTextAnchorPosition();

    // Start or end of document
    if (current_endpoint->IsNullPosition()) {
      if (boundary_behavior == AXBoundaryBehavior::CrossBoundary &&
          ((forwards && AtEndOfAnchor()) || (!forwards && AtStartOfAnchor()))) {
        // Expected behavior is to return a null position for cross-boundary
        // moves that hit the beginning or end of the document
        return std::move(current_endpoint);
      }
      current_endpoint =
          forwards ? initial_endpoint->CreatePositionAtEndOfAnchor()
                   : initial_endpoint->CreatePositionAtStartOfAnchor();
      return was_tree_position ? current_endpoint->AsTreePosition()
                               : std::move(current_endpoint);
    }

    AXNodeTextStyles initial_styles = GetTextStyles();
    if (current_endpoint->GetTextStyles() != initial_styles) {
      // Initial node is at a format boundary. If it's a text position that's
      // not at the start or end of the current anchor, move to the start or
      // end, depending on direction.
      if (!was_tree_position) {
        if (forwards && !initial_endpoint->AtEndOfAnchor())
          return initial_endpoint->CreatePositionAtEndOfAnchor();
        else if (!forwards && !initial_endpoint->AtStartOfAnchor())
          return initial_endpoint->CreatePositionAtStartOfAnchor();
      }

      // We were already at the start or end of a node on a format boundary.
      // If AXBoundaryBehavior::StopIfAlreadyAtBoundary, return here.
      if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary) {
        return was_tree_position ? initial_endpoint->AsTreePosition()
                                 : std::move(initial_endpoint);
      }

      // If we were already at a boundary but moving cross-boundary, use the
      // formats beyond the initial boundary for comparison.
      initial_styles = current_endpoint->GetTextStyles();
      initial_endpoint = current_endpoint->Clone();
    }

    auto next_endpoint = current_endpoint->Clone();
    do {
      if (forwards)
        next_endpoint = next_endpoint->CreateNextTextAnchorPosition();
      else
        next_endpoint = next_endpoint->CreatePreviousTextAnchorPosition();

      if (next_endpoint->IsNullPosition() ||
          next_endpoint->GetTextStyles() != initial_styles)
        break;

      current_endpoint = next_endpoint->Clone();
    } while (true);

    // Moving forwards should leave the position at the end of an anchor.
    // Backwards moves are already at the start of the anchor from
    // CreatePreviousTextAnchorPosition, so there's no need to move again.
    if (forwards)
      current_endpoint = current_endpoint->CreatePositionAtEndOfAnchor();
    else
      DCHECK(current_endpoint->AtStartOfAnchor());

    if (was_tree_position)
      return current_endpoint->AsTreePosition();

    return current_endpoint->AsLeafTextPosition();
  }

  AXPositionInstance CreatePositionAtStartOfDocument() const {
    if (kind_ == AXPositionKind::NULL_POSITION)
      return CreateNullPosition();

    AXPositionInstance iterator = Clone();
    while (!iterator->IsNullPosition()) {
      if (IsDocument(iterator->GetRole()) &&
          iterator->CreateParentPosition()->IsNullPosition()) {
        return iterator->CreatePositionAtStartOfAnchor();
      }
      iterator = iterator->CreateParentPosition();
    }
    return CreateNullPosition();
  }

  AXPositionInstance CreatePositionAtEndOfDocument() const {
    if (kind_ == AXPositionKind::NULL_POSITION)
      return CreateNullPosition();

    AXPositionInstance iterator = Clone();
    while (!iterator->IsNullPosition()) {
      if (IsDocument(iterator->GetRole()) &&
          iterator->CreateParentPosition()->IsNullPosition()) {
        AXPositionInstance tree_position = iterator->AsTreePosition();
        DCHECK(tree_position);
        while (tree_position->AnchorChildCount()) {
          tree_position = tree_position->CreateChildPositionAt(
              tree_position->AnchorChildCount() - 1);
        }
        iterator =
            tree_position->AsLeafTextPosition()->CreatePositionAtEndOfAnchor();
        return iterator;
      }
      iterator = iterator->CreateParentPosition();
    }
    return CreateNullPosition();
  }

  AXPositionInstance CreateChildPositionAt(int child_index) const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (child_index < 0 || child_index >= AnchorChildCount())
      return CreateNullPosition();

    AXTreeID tree_id = AXTreeIDUnknown();
    int32_t child_id = INVALID_ANCHOR_ID;
    AnchorChild(child_index, &tree_id, &child_id);
    DCHECK_NE(tree_id, AXTreeIDUnknown());
    DCHECK_NE(child_id, INVALID_ANCHOR_ID);
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        NOTREACHED();
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION: {
        AXPositionInstance child_position =
            CreateTreePosition(tree_id, child_id, 0 /* child_index */);
        // If the child's anchor is a leaf node, make this a "before text"
        // position.
        if (!child_position->AnchorChildCount())
          child_position->child_index_ = BEFORE_TEXT;
        return child_position;
      }
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(tree_id, child_id, 0 /* text_offset */,
                                  ax::mojom::TextAffinity::kDownstream);
    }

    return CreateNullPosition();
  }

  AXPositionInstance CreateParentPosition() const {
    if (IsNullPosition())
      return CreateNullPosition();

    AXTreeID tree_id = AXTreeIDUnknown();
    int32_t parent_id = INVALID_ANCHOR_ID;
    AnchorParent(&tree_id, &parent_id);
    if (tree_id == AXTreeIDUnknown() || parent_id == INVALID_ANCHOR_ID)
      return CreateNullPosition();

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        NOTREACHED();
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        return CreateTreePosition(tree_id, parent_id, AnchorIndexInParent());
      case AXPositionKind::TEXT_POSITION: {
        // If our parent contains all our text, we need to maintain the affinity
        // and the text offset. Otherwise, we return a position that is either
        // before or after the child. We always recompute the affinity when the
        // position is after the child.
        // Recomputing the affinity is important because even though a text
        // position might unambiguously be at the end of a line, its parent
        // position might be the same as the parent position of the position
        // representing the start of the next line.
        int parent_offset = AnchorTextOffsetInParent();
        ax::mojom::TextAffinity parent_affinity = affinity_;
        if (MaxTextOffset() == MaxTextOffsetInParent()) {
          parent_offset += text_offset_;
        } else if (text_offset_ > 0) {
          parent_offset += MaxTextOffsetInParent();
          parent_affinity = ax::mojom::TextAffinity::kDownstream;
        }

        AXPositionInstance parent_position = CreateTextPosition(
            tree_id, parent_id, parent_offset, parent_affinity);
        if (parent_position->IsNullPosition()) {
          // Workaround: When the autofill feature populates a text field, it
          // doesn't immediately update its value, which causes the text inside
          // the user-agent shadow DOM to be different than the text in the text
          // field itself. As a result, the parent_offset calculated above might
          // appear to be temporarily invalid.
          // TODO(nektar): Fix this better by ensuring that the text field's
          // hypertext is always kept up to date.
          parent_position =
              CreateTextPosition(tree_id, parent_id, 0 /* text_offset */,
                                 ax::mojom::TextAffinity::kDownstream);
        }

        // We check if the parent position has introduced ambiguity as to
        // whether it refers to the end of the current or the start of the next
        // line. We do this check by creating the parent position and testing if
        // it is erroneously at the start of the next line. We could not have
        // checked if the child was at the end of the line, because our line end
        // testing logic takes into account line breaks, which don't apply in
        // this situation.
        if (text_offset_ == MaxTextOffset() && parent_position->AtStartOfLine())
          parent_position->affinity_ = ax::mojom::TextAffinity::kUpstream;
        return parent_position;
      }
    }

    return CreateNullPosition();
  }

  // Creates a position using the next text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreateNextTextAnchorPosition() const {
    return CreateNextTextAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Creates a position using the previous text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreatePreviousTextAnchorPosition() const {
    return CreatePreviousTextAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Returns a text position located right before the next character (from this
  // position) in the tree's text representation, following these conditions:
  //   - If this position is at the end of its anchor, normalize it to the start
  //   of the next text anchor, regardless of the position's affinity. Both
  //   positions are equal when compared, but we consider the start of an anchor
  //   to be a position BEFORE its first character and the end to be AFTER its
  //   last character.
  //   - Skip any empty text anchors; they're "invisible" to the text
  //   representation and the next character could be ahead.
  //   - Return a null position if there is no next character forward.
  // Don't return a leaf equivalent position, but try and return a position that
  // has the same anchor as the current position if the resulting position is
  // enclosed by the same anchor.
  AXPositionInstance AsPositionBeforeCharacter() const {
    AXPositionInstance text_position = AsLeafTextPositionBeforeCharacter();

    // If possible, return a position anchored at the current position. This is
    // necessary because we don't want to return any position that might be in
    // the shadow DOM or a position anchored at a node that is not visible to
    // platform APIs, if the original position didn't meet any of these
    // criteria.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor())
      text_position = std::move(common_ancestor);

    return text_position;
  }

  // Returns a text position located right after the previous character (from
  // this position) in the tree's text representation.
  // See `AsPositionBeforeCharacter`, as this is its "reversed" version.
  AXPositionInstance AsPositionAfterCharacter() const {
    AXPositionInstance text_position = AsLeafTextPositionAfterCharacter();

    // If possible, return a position anchored at the current position. This is
    // necessary because we don't want to return any position that might be in
    // the shadow DOM or a position anchored at a node that is not visible to
    // platform APIs, if the original position didn't meet any of these
    // criteria.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor())
      text_position = std::move(common_ancestor);

    return text_position;
  }

  // Same as `AsPositionBeforeCharacter`, but returns the leaf equivalent text
  // position.
  AXPositionInstance AsLeafTextPositionBeforeCharacter() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    // This loop satisfies all the conditions described above.
    while (text_position->AtEndOfAnchor())
      text_position = text_position->CreateNextTextAnchorPosition();
    return text_position;
  }

  // Same as `AsPositionAfterCharacter`, but returns the leaf equivalent text
  // position.
  AXPositionInstance AsLeafTextPositionAfterCharacter() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    while (text_position->AtStartOfAnchor()) {
      text_position = text_position->CreatePreviousTextAnchorPosition();
      text_position = text_position->CreatePositionAtEndOfAnchor();
    }
    return text_position;
  }

  AXPositionInstance CreateNextCharacterPosition(
      AXBoundaryBehavior boundary_behavior) const {
    DCHECK_NE(boundary_behavior, AXBoundaryBehavior::StopIfAlreadyAtBoundary)
        << "StopIfAlreadyAtBoundary is unreasonable for character boundaries.";
    if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary &&
        AtEndOfAnchor())
      return Clone();

    const bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsPositionBeforeCharacter();
    // There is no next character position.
    if (text_position->IsNullPosition())
      return text_position;

    ++text_position->text_offset_;
    DCHECK_LE(text_position->text_offset_, text_position->MaxTextOffset());
    // Even if the position's affinity was upstream, moving to the next
    // character should inevitably reset it to downstream.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;

    if (was_tree_position)
      return text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreatePreviousCharacterPosition(
      AXBoundaryBehavior boundary_behavior) const {
    DCHECK_NE(boundary_behavior, AXBoundaryBehavior::StopIfAlreadyAtBoundary)
        << "StopIfAlreadyAtBoundary is unreasonable for character boundaries.";
    if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary &&
        AtStartOfAnchor())
      return Clone();

    const bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsPositionAfterCharacter();
    // There is no previous character position.
    if (text_position->IsNullPosition())
      return text_position;

    --text_position->text_offset_;
    DCHECK_GE(text_position->text_offset_, 0);
    // Even if the moved position is at the beginning of the line, the
    // affinity is defaulted to downstream for simplicity.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;

    if (was_tree_position)
      return text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateNextWordStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfWord()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    std::vector<int32_t> word_starts = text_position->GetWordStartOffsets();
    auto iterator =
        std::upper_bound(word_starts.begin(), word_starts.end(),
                         static_cast<int32_t>(text_position->text_offset_));
    do {
      if (iterator == word_starts.end()) {
        // Ignore any nodes with no text or no word boundaries.
        do {
          text_position = text_position->CreateNextTextAnchorPosition();
          if (text_position->IsNullPosition()) {
            if (AtEndOfAnchor() &&
                boundary_behavior == AXBoundaryBehavior::CrossBoundary)
              return text_position;
            return CreatePositionAtEndOfAnchor();
          }
        } while (!text_position->MaxTextOffset() ||
                 text_position->GetWordStartOffsets().empty());

        word_starts = text_position->GetWordStartOffsets();
        DCHECK(!word_starts.empty());
        iterator =
            std::upper_bound(word_starts.begin(), word_starts.end(),
                             static_cast<int32_t>(text_position->text_offset_));
        text_position->text_offset_ = static_cast<int>(word_starts[0]);
      } else {
        text_position->text_offset_ = static_cast<int>(*iterator);
        text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      }

      // Continue searching for the next word start until the next logical text
      // position is reached.
    } while (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
             *this == *text_position);

    // If the word boundary is in the same subtree, return a position rooted
    // at the current position. This is necessary because we don't want to
    // return any position that might be in the shadow DOM if the original
    // position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreatePreviousWordStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfWord()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    std::vector<int32_t> word_starts = text_position->GetWordStartOffsets();
    auto iterator =
        std::lower_bound(word_starts.begin(), word_starts.end(),
                         static_cast<int32_t>(text_position->text_offset_));
    do {
      if (word_starts.empty() || iterator == word_starts.begin()) {
        // Ignore any nodes with no text or no word boundaries.
        do {
          text_position = text_position->CreatePreviousTextAnchorPosition()
                              ->CreatePositionAtEndOfAnchor();
          if (text_position->IsNullPosition()) {
            if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
              return CreatePositionAtStartOfAnchor();
            return text_position;
          }
        } while (!text_position->MaxTextOffset() ||
                 text_position->GetWordStartOffsets().empty());

        word_starts = text_position->GetWordStartOffsets();
        DCHECK(!word_starts.empty());
        iterator =
            std::upper_bound(word_starts.begin(), word_starts.end(),
                             static_cast<int32_t>(text_position->text_offset_));
        text_position->text_offset_ =
            static_cast<int>(*(word_starts.end() - 1));
      } else {
        text_position->text_offset_ = static_cast<int>(*(--iterator));
        text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      }

      // Continue searching for the previous word start until the next logical
      // text position is reached.
    } while (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
             *this == *text_position);

    // If the word boundary is in the same subtree, return a position rooted
    // at the current position. This is necessary because we don't want to
    // return any position that might be in the shadow DOM if the original
    // position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // Word end positions are one past the last character of the word.
  AXPositionInstance CreateNextWordEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfWord()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfLine())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    std::vector<int32_t> word_ends = text_position->GetWordEndOffsets();
    auto iterator =
        std::upper_bound(word_ends.begin(), word_ends.end(),
                         static_cast<int32_t>(text_position->text_offset_));
    do {
      if (iterator == word_ends.end()) {
        // Ignore any nodes with no text or no word boundaries.
        do {
          text_position = text_position->CreateNextTextAnchorPosition();
          if (text_position->IsNullPosition()) {
            if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
              return CreatePositionAtEndOfAnchor();
            return text_position;
          }
        } while (!text_position->MaxTextOffset() ||
                 text_position->GetWordEndOffsets().empty());

        word_ends = text_position->GetWordEndOffsets();
        DCHECK(!word_ends.empty());
        iterator =
            std::upper_bound(word_ends.begin(), word_ends.end(),
                             static_cast<int32_t>(text_position->text_offset_));
        text_position->text_offset_ = static_cast<int>(word_ends[0]);
      } else {
        text_position->text_offset_ = static_cast<int>(*iterator);
        text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      }

      // Continue searching for the next word end until the next logical text
      // position is reached.
    } while (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
             *this == *text_position);

    // If the word boundary is in the same subtree, return a position rooted
    // at the current position. This is necessary because we don't want to
    // return any position that might be in the shadow DOM if the original
    // position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // Word end positions are one past the last character of the word.
  AXPositionInstance CreatePreviousWordEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfWord()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfLine())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    std::vector<int32_t> word_ends = text_position->GetWordEndOffsets();
    auto iterator =
        std::lower_bound(word_ends.begin(), word_ends.end(),
                         static_cast<int32_t>(text_position->text_offset_));
    do {
      if (word_ends.empty() || iterator == word_ends.begin()) {
        // Ignore any nodes with no text or no word boundaries.
        do {
          text_position = text_position->CreatePreviousTextAnchorPosition()
                              ->CreatePositionAtEndOfAnchor();
          if (text_position->IsNullPosition()) {
            if (AtStartOfAnchor() &&
                boundary_behavior == AXBoundaryBehavior::CrossBoundary)
              return text_position;
            return CreatePositionAtStartOfAnchor();
          }
        } while (!text_position->MaxTextOffset() ||
                 text_position->GetWordStartOffsets().empty());

        word_ends = text_position->GetWordEndOffsets();
        DCHECK(!word_ends.empty());
        iterator =
            std::lower_bound(word_ends.begin(), word_ends.end(),
                             static_cast<int32_t>(text_position->text_offset_));
        text_position->text_offset_ = static_cast<int>(*(word_ends.end() - 1));
      } else {
        text_position->text_offset_ = static_cast<int>(*(--iterator));
        text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      }

      // Continue searching for the previous word end until the next logical
      // text position is reached.
    } while (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
             *this == *text_position);

    // If the word boundary is in the same subtree, return a position rooted
    // at the current position. This is necessary because we don't want to
    // return any position that might be in the shadow DOM if the original
    // position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateNextLineStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfLine()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    do {
      text_position = text_position->CreateNextTextAnchorPosition();
      if (text_position->IsNullPosition()) {
        if (AtEndOfAnchor() &&
            boundary_behavior == AXBoundaryBehavior::CrossBoundary)
          return text_position;
        return CreatePositionAtEndOfAnchor();
      }

      // Continue searching for the next line start until the next logical text
      // position is reached.
    } while (
        !text_position->AtStartOfLine() ||
        (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
         *this == *text_position));

    // If the line boundary is in the same subtree, return a position rooted at
    // the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreatePreviousLineStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfLine()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    do {
      if (text_position->AtStartOfAnchor()) {
        text_position = text_position->CreatePreviousTextAnchorPosition();
      } else {
        text_position = text_position->CreatePositionAtStartOfAnchor();
      }

      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtStartOfAnchor();
        return text_position;
      }

      // Continue searching for the previous line start until the next logical
      // text position is reached.
    } while (
        !text_position->AtStartOfLine() ||
        (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
         *this == *text_position));

    // If the line boundary is in the same subtree, return a position rooted at
    // the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // Line end positions are one past the last character of the line, excluding
  // any white space or newline characters that separate the lines.
  AXPositionInstance CreateNextLineEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfLine()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfLine())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    do {
      if (text_position->AtEndOfAnchor()) {
        text_position = text_position->CreateNextTextAnchorPosition()
                            ->CreatePositionAtEndOfAnchor();
      } else {
        text_position = text_position->CreatePositionAtEndOfAnchor();
      }

      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtEndOfAnchor();
        return text_position;
      }

      // Continue searching for the next line end until the next logical text
      // position is reached.
    } while (
        !text_position->AtEndOfLine() ||
        (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
         *this == *text_position));

    // If the line boundary is in the same subtree, return a position rooted at
    // the current position. This is necessary because we don't want to return
    // any position that might be in the shadow DOM if the original position was
    // not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // Line end positions are one past the last character of the line, excluding
  // any white space or newline characters separating the lines.
  AXPositionInstance CreatePreviousLineEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfLine()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfLine())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    do {
      text_position = text_position->CreatePreviousTextAnchorPosition()
                          ->CreatePositionAtEndOfAnchor();
      if (text_position->IsNullPosition()) {
        if (AtStartOfAnchor() &&
            boundary_behavior == AXBoundaryBehavior::CrossBoundary)
          return text_position;
        return CreatePositionAtStartOfAnchor();
      }

      // Continue searching for the previous line end until the next logical
      // text position is reached.
    } while (
        !text_position->AtEndOfLine() ||
        (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
         *this == *text_position));

    // If the line boundary is in the same subtree, return a position rooted at
    // the current position. This is necessary because we don't want to return
    // any position that might be in the shadow DOM if the original position was
    // not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateNextParagraphStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfParagraph()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    do {
      text_position = text_position->CreateNextTextAnchorPosition();
      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtEndOfAnchor();
        return text_position;
      }

      // Continue searching for the next paragraph start until the next logical
      // text position is reached.
    } while (
        !text_position->AtStartOfParagraph() ||
        (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
         *this == *text_position));

    // If the paragraph boundary is in the same subtree, return a position
    // rooted at the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreatePreviousParagraphStartPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtStartOfParagraph()) {
      AXPositionInstance clone = Clone();
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return clone;
    }

    do {
      if (text_position->AtStartOfAnchor()) {
        text_position = text_position->CreatePreviousTextAnchorPosition();
      } else {
        text_position = text_position->CreatePositionAtStartOfAnchor();
      }

      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtStartOfAnchor();
        return text_position;
      }

      // Continue searching for the previous paragraph start until the next
      // logical text position is reached.
    } while (
        !text_position->AtStartOfParagraph() ||
        (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
         *this == *text_position));

    // If the paragraph boundary is in the same subtree, return a position
    // rooted at the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateNextParagraphEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfParagraph()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current paragraph or the start of the next paragraph, affinity
      // should be reset in order to get consistent output from this function
      // regardless of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfParagraph())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    do {
      if (text_position->AtEndOfAnchor()) {
        text_position = text_position->CreateNextTextAnchorPosition()
                            ->CreatePositionAtEndOfAnchor();
      } else {
        text_position = text_position->CreatePositionAtEndOfAnchor();
      }

      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtEndOfAnchor();
        return text_position;
      }

      // Continue searching for the next paragraph end until the next logical
      // text position is reached.
    } while (
        !text_position->AtEndOfParagraph() ||
        (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
         *this == *text_position));

    // If the paragraph boundary is in the same subtree, return a position
    // rooted at the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtEndOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreatePreviousParagraphEndPosition(
      AXBoundaryBehavior boundary_behavior) const {
    bool was_tree_position = IsTreePosition();
    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->IsNullPosition())
      return text_position;
    if (boundary_behavior == AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
        text_position->AtEndOfParagraph()) {
      AXPositionInstance clone = Clone();
      // If there is no ambiguity as to whether the position is at the end of
      // the current line or the start of the next line, affinity should be
      // reset in order to get consistent output from this function regardless
      // of input affinity.
      clone->affinity_ = ax::mojom::TextAffinity::kDownstream;
      if (clone->AtStartOfParagraph())
        clone->affinity_ = ax::mojom::TextAffinity::kUpstream;
      return clone;
    }

    do {
      text_position = text_position->CreatePreviousTextAnchorPosition()
                          ->CreatePositionAtEndOfAnchor();
      if (text_position->IsNullPosition()) {
        if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary)
          return CreatePositionAtStartOfAnchor();
        return text_position;
      }

      // Continue searching for the previous paragraph end until the next
      // logical text position is reached.
    } while (
        !text_position->AtEndOfParagraph() ||
        (boundary_behavior != AXBoundaryBehavior::StopIfAlreadyAtBoundary &&
         *this == *text_position));

    // If the paragraph boundary is in the same subtree, return a position
    // rooted at the current position.
    // This is necessary because we don't want to return any position that might
    // be in the shadow DOM if the original position was not.
    AXPositionInstance common_ancestor =
        text_position->LowestCommonAncestor(*this);
    if (GetAnchor() == common_ancestor->GetAnchor()) {
      text_position = std::move(common_ancestor);
    } else if (boundary_behavior == AXBoundaryBehavior::StopAtAnchorBoundary) {
      return CreatePositionAtStartOfAnchor();
    }

    if (was_tree_position)
      text_position = text_position->AsTreePosition();
    return text_position;
  }

  // TODO(nektar): Add sentence navigation methods.

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreateNextAnchorPosition() const {
    return CreateNextAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreatePreviousAnchorPosition() const {
    return CreatePreviousAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Returns an optional integer indicating the logical order of this position
  // compared to another position or returns an empty optional if the positions
  // are not comparable. Any text position at the same character location is
  // logically equivalent although they may be on different anchors or have
  // different text offsets. Positions are not comparable when one position is
  // null and the other is not or if the positions do not have any common
  // ancestor.
  //    0: if this position is logically equivalent to the other position
  //   <0: if this position is logically less than the other position
  //   >0: if this position is logically greater than the other position
  base::Optional<int> CompareTo(
      const AXPosition<AXPositionType, AXNodeType>& other) const {
    if (this->IsNullPosition() && other.IsNullPosition())
      return base::Optional<int>(0);
    if (this->IsNullPosition() || other.IsNullPosition())
      return base::Optional<int>(base::nullopt);

    // It is potentially costly to compute the parent position of a text
    // position, whilst computing the parent position of a tree position is
    // really inexpensive. In order to find the lowest common ancestor,
    // especially if that ancestor is all the way up to the root of the tree,
    // this will need to be done repeatedly. We avoid the performance hit by
    // converting both positions to tree positions and only falling back to text
    // positions if both are text positions and the lowest common ancestor is
    // not one of their anchors. Essentially, the question we need to answer is:
    // "When are two non equivalent positions going to have the same lowest
    // common ancestor position when converted to tree positions?" The answer is
    // when they are both text positions and they either have the same anchor,
    // or one is the ancestor of the other.
    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>> this_tree_position =
        this->AsTreePosition();
    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>
        other_tree_position = other.AsTreePosition();
    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>
        this_tree_position_ancestor =
            this_tree_position->LowestCommonAncestor(*other_tree_position);
    std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>
        other_tree_position_ancestor =
            other_tree_position->LowestCommonAncestor(*this_tree_position);
    DCHECK_EQ(this_tree_position_ancestor->GetAnchor(),
              other_tree_position_ancestor->GetAnchor());
    if (this_tree_position_ancestor->IsNullPosition())
      return base::Optional<int>(base::nullopt);
    DCHECK(this_tree_position_ancestor->IsTreePosition() &&
           other_tree_position_ancestor->IsTreePosition());

    // Attempt to avoid recomputing the lowest common ancestor because we may
    // already have its anchor in which case just find the text offset.
    if (this->IsTextPosition() && other.IsTextPosition()) {
      // This text position's anchor is the common ancestor of the other text
      // position's anchor.
      if (this->GetAnchor() == other_tree_position_ancestor->GetAnchor()) {
        std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>
            other_text_position = other.Clone();
        while (other_text_position->GetAnchor() != this->GetAnchor())
          other_text_position = other_text_position->CreateParentPosition();
        return base::Optional<int>(this->text_offset_ -
                                   other_text_position->text_offset_);
      }

      // The other text position's anchor is the common ancestor of this text
      // position's anchor.
      if (other.GetAnchor() == this_tree_position_ancestor->GetAnchor()) {
        std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>
            this_text_position = this->Clone();
        while (this_text_position->GetAnchor() != other.GetAnchor())
          this_text_position = this_text_position->CreateParentPosition();
        return base::Optional<int>(this_text_position->text_offset_ -
                                   other.text_offset_);
      }

      // All optimizations failed. Fall back to comparing text positions with
      // the common text position ancestor.
      std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>
          this_text_position_ancestor = this->LowestCommonAncestor(other);
      std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>
          other_text_position_ancestor = other.LowestCommonAncestor(*this);
      DCHECK(this_text_position_ancestor->IsTextPosition());
      DCHECK(other_text_position_ancestor->IsTextPosition());
      DCHECK_EQ(this_text_position_ancestor->GetAnchor(),
                other_text_position_ancestor->GetAnchor());

      // TODO - This does not take into account |affinity_|, so we may return
      // a false positive when comparing at the end of a line.
      // For example :
      // ++1 kRootWebArea
      // ++++2 kTextField "Line 1\nLine 2"
      // ++++++3 kStaticText "Line 1"
      // ++++++++4 kInlineTextBox "Line 1"
      // ++++++5 kLineBreak "\n"
      // ++++++6 kStaticText "Line 2"
      // ++++++++7 kInlineTextBox "Line 2"
      //
      // TextPosition anchor_id=5 text_offset=1
      // affinity=downstream annotated_text=\n<>
      //
      // TextPosition anchor_id=7 text_offset=0
      // affinity=downstream annotated_text=<L>ine 2
      //
      // |LowestCommonAncestor| for both will be :
      // TextPosition anchor_id=2 text_offset=7
      // ... except anchor_id=5 creates a kUpstream position, while
      // anchor_id=7 creates a kDownstream position.
      return base::Optional<int>(this_text_position_ancestor->text_offset_ -
                                 other_text_position_ancestor->text_offset_);
    }

    return base::Optional<int>(this_tree_position_ancestor->child_index() -
                               other_tree_position_ancestor->child_index());
  }

  // Returns the length of the text that is present inside the anchor node,
  // including any text found in descendant text nodes.
  int MaxTextOffset() const {
    if (IsNullPosition())
      return INVALID_INDEX;
    return static_cast<int>(GetText().length());
  }

  // Abstract methods.

  // Determines if the anchor containing this position is a <br> or a text
  // object whose parent's anchor is an enclosing <br>.
  virtual bool IsInLineBreak() const = 0;

  // Determines if the anchor containing this position is a text object.
  virtual bool IsInTextObject() const = 0;

  // Determines if the text representation of this position's anchor contains
  // only whitespace characters; <br> objects span a single '\n' character, so
  // positions inside line breaks are also considered "in whitespace".
  virtual bool IsInWhiteSpace() const = 0;

  // Returns the text that is present inside the anchor node, where the
  // representation of text found in descendant nodes depends on the platform.
  // For example some platforms may include descendant text while while other
  // platforms may use a special character to represent descendant text.
  virtual base::string16 GetText() const = 0;

 protected:
  AXPosition() = default;
  AXPosition(const AXPosition<AXPositionType, AXNodeType>& other) = default;
  virtual AXPosition<AXPositionType, AXNodeType>& operator=(
      const AXPosition<AXPositionType, AXNodeType>& other) = default;

  virtual void Initialize(AXPositionKind kind,
                          AXTreeID tree_id,
                          int32_t anchor_id,
                          int child_index,
                          int text_offset,
                          ax::mojom::TextAffinity affinity) {
    kind_ = kind;
    tree_id_ = tree_id;
    anchor_id_ = anchor_id;
    child_index_ = child_index;
    text_offset_ = text_offset;
    affinity_ = affinity;

    if (!GetAnchor() || kind_ == AXPositionKind::NULL_POSITION ||
        (kind_ == AXPositionKind::TREE_POSITION &&
         (child_index_ != BEFORE_TEXT &&
          (child_index_ < 0 || child_index_ > AnchorChildCount()))) ||
        (kind_ == AXPositionKind::TEXT_POSITION &&
         (text_offset_ < 0 || text_offset_ > MaxTextOffset()))) {
      // Reset to the null position.
      kind_ = AXPositionKind::NULL_POSITION;
      tree_id_ = AXTreeIDUnknown();
      anchor_id_ = INVALID_ANCHOR_ID;
      child_index_ = INVALID_INDEX;
      text_offset_ = INVALID_OFFSET;
      affinity_ = ax::mojom::TextAffinity::kDownstream;
    }
  }

  // Returns the character offset inside our anchor's parent at which our text
  // starts.
  int AnchorTextOffsetInParent() const {
    if (IsNullPosition())
      return INVALID_OFFSET;

    // Calculate how much text there is to the left of this anchor.
    AXPositionInstance tree_position = AsTreePosition();
    DCHECK(tree_position);
    AXPositionInstance parent_position = tree_position->CreateParentPosition();
    DCHECK(parent_position);
    if (parent_position->IsNullPosition())
      return 0;

    int offset_in_parent = 0;
    for (int i = 0; i < parent_position->child_index(); ++i) {
      AXPositionInstance child = parent_position->CreateChildPositionAt(i);
      DCHECK(child);
      offset_in_parent += child->MaxTextOffsetInParent();
    }
    return offset_in_parent;
  }

  // Abstract methods.
  virtual void AnchorChild(int child_index,
                           AXTreeID* tree_id,
                           int32_t* child_id) const = 0;
  virtual int AnchorChildCount() const = 0;
  virtual int AnchorIndexInParent() const = 0;
  virtual base::stack<AXNodeType*> GetAncestorAnchors() const = 0;
  virtual void AnchorParent(AXTreeID* tree_id, int32_t* parent_id) const = 0;
  virtual AXNodeType* GetNodeInTree(AXTreeID tree_id,
                                    int32_t node_id) const = 0;

  // Returns the length of text that this anchor node takes up in its parent.
  // On some platforms, embedded objects are represented in their parent with a
  // single embedded object character.
  virtual int MaxTextOffsetInParent() const { return MaxTextOffset(); }

  // Determines if the anchor containing this position produces a hard line
  // break in the text representation, e.g. a block level element or a <br>.
  virtual bool IsInLineBreakingObject() const = 0;

  virtual ax::mojom::Role GetRole() const = 0;
  virtual AXNodeTextStyles GetTextStyles() const = 0;
  virtual std::vector<int32_t> GetWordStartOffsets() const = 0;
  virtual std::vector<int32_t> GetWordEndOffsets() const = 0;
  virtual int32_t GetNextOnLineID(int32_t node_id) const = 0;
  virtual int32_t GetPreviousOnLineID(int32_t node_id) const = 0;

 private:
  // Defines the relationship between positions during traversal.
  // For example, moving from a descendant to an ancestor, is a kAncestor move.
  enum class AXMoveType {
    kAncestor,
    kDescendant,
    kSibling,
  };

  // Defines the direction of position movement, either next / previous in tree.
  enum class AXMoveDirection {
    kNextInTree,
    kPreviousInTree,
  };

  // type of predicate function called during anchor navigation.
  // When the predicate returns |true|, the navigation stops and returns a
  // null position object.
  using AbortMovePredicate = base::RepeatingCallback<bool(
      const AXPosition<AXPositionType, AXNodeType>& move_from,
      const AXPosition<AXPositionType, AXNodeType>& move_to,
      const AXMoveType type,
      const AXMoveDirection direction)>;

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreateNextAnchorPosition(
      const AbortMovePredicate abort_predicate) const {
    if (IsNullPosition())
      return CreateNullPosition();

    if (AnchorChildCount()) {
      AXPositionInstance child_position = nullptr;

      if (IsTreePosition()) {
        if (child_index_ < AnchorChildCount())
          child_position = CreateChildPositionAt(child_index_);
      } else {
        // We have to find the child node that encompasses the current text
        // offset.
        AXPositionInstance tree_position = AsTreePosition();
        DCHECK(tree_position);
        int child_index = tree_position->child_index_;
        if (child_index < tree_position->AnchorChildCount())
          child_position = tree_position->CreateChildPositionAt(child_index);
      }

      if (child_position) {
        if (abort_predicate.Run(*this, *child_position, AXMoveType::kDescendant,
                                AXMoveDirection::kNextInTree)) {
          return CreateNullPosition();
        }
        return std::move(child_position);
      }
    }

    AXPositionInstance current_position = Clone();
    AXPositionInstance parent_position = CreateParentPosition();
    while (!parent_position->IsNullPosition()) {
      // Get the next sibling if it exists, otherwise move up to the parent's
      // next sibling.
      int index_in_parent = current_position->AnchorIndexInParent();
      if (index_in_parent + 1 < parent_position->AnchorChildCount()) {
        AXPositionInstance next_sibling =
            parent_position->CreateChildPositionAt(index_in_parent + 1);
        DCHECK(next_sibling && !next_sibling->IsNullPosition());

        if (abort_predicate.Run(*current_position, *next_sibling,
                                AXMoveType::kSibling,
                                AXMoveDirection::kNextInTree)) {
          return CreateNullPosition();
        }
        return std::move(next_sibling);
      }

      if (abort_predicate.Run(*current_position, *parent_position,
                              AXMoveType::kAncestor,
                              AXMoveDirection::kNextInTree)) {
        return CreateNullPosition();
      }

      current_position = std::move(parent_position);
      parent_position = current_position->CreateParentPosition();
    }

    return CreateNullPosition();
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreatePreviousAnchorPosition(
      const AbortMovePredicate abort_predicate) const {
    if (IsNullPosition())
      return CreateNullPosition();

    AXPositionInstance parent_position = CreateParentPosition();
    if (parent_position->IsNullPosition())
      return CreateNullPosition();

    // Get the previous sibling's deepest last child if a previous sibling
    // exists, otherwise move up to the parent.
    int index_in_parent = AnchorIndexInParent();
    if (index_in_parent <= 0) {
      if (abort_predicate.Run(*this, *parent_position, AXMoveType::kAncestor,
                              AXMoveDirection::kPreviousInTree)) {
        return CreateNullPosition();
      }

      return std::move(parent_position);
    }

    AXPositionInstance leaf =
        parent_position->CreateChildPositionAt(index_in_parent - 1);
    if (!leaf->IsNullPosition() &&
        abort_predicate.Run(*this, *leaf, AXMoveType::kSibling,
                            AXMoveDirection::kPreviousInTree)) {
      return CreateNullPosition();
    }

    while (!leaf->IsNullPosition() && leaf->AnchorChildCount()) {
      parent_position = std::move(leaf);
      leaf = parent_position->CreateChildPositionAt(
          parent_position->AnchorChildCount() - 1);

      if (abort_predicate.Run(*parent_position, *leaf, AXMoveType::kDescendant,
                              AXMoveDirection::kPreviousInTree)) {
        return CreateNullPosition();
      }
    }

    return std::move(leaf);
  }

  // Creates a position using the next text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreateNextTextAnchorPosition(
      const AbortMovePredicate abort_predicate) const {
    // If this is an ancestor text position, resolve to its leaf text position.
    if (IsTextPosition() && AnchorChildCount())
      return AsLeafTextPosition();

    AXPositionInstance next_leaf = CreateNextAnchorPosition(abort_predicate);
    while (!next_leaf->IsNullPosition() && next_leaf->AnchorChildCount()) {
      next_leaf = next_leaf->CreateNextAnchorPosition(abort_predicate);
    }

    DCHECK(next_leaf);
    return next_leaf->AsLeafTextPosition();
  }

  // Creates a position using the previous text-only node as its anchor.
  // Assumes that text-only nodes are leaf nodes.
  AXPositionInstance CreatePreviousTextAnchorPosition(
      const AbortMovePredicate abort_predicate) const {
    // If this is an ancestor text position, resolve to its leaf text position.
    if (IsTextPosition() && AnchorChildCount())
      return AsLeafTextPosition();

    AXPositionInstance previous_leaf =
        CreatePreviousAnchorPosition(abort_predicate);
    while (!previous_leaf->IsNullPosition() &&
           previous_leaf->AnchorChildCount()) {
      previous_leaf =
          previous_leaf->CreatePreviousAnchorPosition(abort_predicate);
    }

    DCHECK(previous_leaf);
    return previous_leaf->AsLeafTextPosition();
  }

  // Default behavior is to never abort
  static bool DefaultAbortMovePredicate(
      const AXPosition<AXPositionType, AXNodeType>& move_from,
      const AXPosition<AXPositionType, AXNodeType>& move_to,
      const AXMoveType move_type,
      const AXMoveDirection direction) {
    return false;
  }

  // AbortMovePredicate function used to detect paragraph boundaries.
  static bool AbortMoveAtParagraphBoundary(
      const AXPosition<AXPositionType, AXNodeType>& move_from,
      const AXPosition<AXPositionType, AXNodeType>& move_to,
      const AXMoveType move_type,
      const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition())
      return true;

    const bool move_from_break = move_from.IsInLineBreakingObject();
    const bool move_to_break = move_to.IsInLineBreakingObject();

    bool potential_paragraph_boundary = false;
    switch (move_type) {
      case AXMoveType::kAncestor:
        // For Ancestor moves, only abort when exiting a block descendant.
        // We don't care if the ancestor is a block or not, since the
        // descendant is contained by it.
        potential_paragraph_boundary = move_from_break;
        break;
      case AXMoveType::kDescendant:
        // For Descendant moves, only abort when entering a block descendant.
        // We don't care if the ancestor is a block or not, since the
        // descendant is contained by it.
        potential_paragraph_boundary = move_to_break;
        break;
      case AXMoveType::kSibling:
        // For Sibling moves, abort if at least one of the siblings are a block,
        // because that would mean exiting and/or entering a block.
        potential_paragraph_boundary = move_from_break || move_to_break;
        break;
    }

    // If there's a sequence of whitespace-only blocks, collapse so only the
    // trailing whitespace-only block is considered a paragraph boundary.
    if (potential_paragraph_boundary) {
      switch (direction) {
        case AXMoveDirection::kNextInTree: {
          // Forward navigation, collapse trailing whitespace only nodes.
          // Move into all whitespace only blocks that are available
          // before aborting.
          if (move_to.IsInWhiteSpace())
            return false;
          break;
        }
        case AXMoveDirection::kPreviousInTree: {
          // Reverse navigation, skip over all whitespace nodes unless
          // it is the last trailing whitespace node, signaling the end
          // of a previous paragraph.
          if (move_from.IsInWhiteSpace())
            return false;
          break;
        }
      }
      return true;
    }

    return false;
  }

  AXPositionKind kind_;
  AXTreeID tree_id_;
  int32_t anchor_id_;

  // For text positions, |child_index_| is initially set to |-1| and only
  // computed on demand. The same with tree positions and |text_offset_|.
  int child_index_;
  int text_offset_;

  // TODO(nektar): Get rid of affinity and make Blink handle affinity
  // internally since inline text objects don't span lines.
  ax::mojom::TextAffinity affinity_;
};

template <class AXPositionType, class AXNodeType>
const int32_t AXPosition<AXPositionType, AXNodeType>::INVALID_ANCHOR_ID;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::BEFORE_TEXT;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::INVALID_INDEX;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::INVALID_OFFSET;

template <class AXPositionType, class AXNodeType>
bool operator==(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() == 0;
}

template <class AXPositionType, class AXNodeType>
bool operator!=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() != 0;
}

template <class AXPositionType, class AXNodeType>
bool operator<(const AXPosition<AXPositionType, AXNodeType>& first,
               const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() < 0;
}

template <class AXPositionType, class AXNodeType>
bool operator<=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() <= 0;
}

template <class AXPositionType, class AXNodeType>
bool operator>(const AXPosition<AXPositionType, AXNodeType>& first,
               const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() > 0;
}

template <class AXPositionType, class AXNodeType>
bool operator>=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const base::Optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() >= 0;
}

template <class AXPositionType, class AXNodeType>
std::ostream& operator<<(
    std::ostream& stream,
    const AXPosition<AXPositionType, AXNodeType>& position) {
  return stream << position.ToString();
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_POSITION_H_
