// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"

#include <limits>

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

BlinkAXEventIntent BlinkAXEventIntent::FromClearedSelection(
    const SetSelectionBy set_selection_by) {
  // |text_boundary| and |move_direction| are not used in this case.
  return BlinkAXEventIntent(ax::mojom::blink::Command::kClearSelection,
                            ax::mojom::blink::TextBoundary::kCharacter,
                            ax::mojom::blink::MoveDirection::kForward);
}

BlinkAXEventIntent BlinkAXEventIntent::FromModifiedSelection(
    const SelectionModifyAlteration alter,
    const SelectionModifyDirection direction,
    const TextGranularity granularity,
    const SetSelectionBy set_selection_by,
    const TextDirection direction_of_selection) {
  ax::mojom::blink::Command command;
  switch (alter) {
    case SelectionModifyAlteration::kExtend:
      // Includes the case when the existing selection has been shrunk.
      command = ax::mojom::blink::Command::kExtendSelection;
      break;
    case SelectionModifyAlteration::kMove:
      // The existing selection has been move by a specific |granularity|, e.g.
      // the caret has been moved to the beginning of the next word.
      command = ax::mojom::blink::Command::kMoveSelection;
      break;
  }

  ax::mojom::blink::MoveDirection move_direction;
  switch (direction) {
    case SelectionModifyDirection::kBackward:
      move_direction = ax::mojom::blink::MoveDirection::kBackward;
      break;
    case SelectionModifyDirection::kForward:
      move_direction = ax::mojom::blink::MoveDirection::kForward;
      break;
    case SelectionModifyDirection::kLeft:
      move_direction = IsLtr(direction_of_selection)
                           ? ax::mojom::blink::MoveDirection::kBackward
                           : ax::mojom::blink::MoveDirection::kForward;
      break;
    case SelectionModifyDirection::kRight:
      move_direction = IsLtr(direction_of_selection)
                           ? ax::mojom::blink::MoveDirection::kForward
                           : ax::mojom::blink::MoveDirection::kBackward;
      break;
  }

  ax::mojom::blink::TextBoundary text_boundary;
  switch (granularity) {
    case TextGranularity::kCharacter:
      text_boundary = ax::mojom::blink::TextBoundary::kCharacter;
      break;
    case TextGranularity::kWord:
      switch (move_direction) {
        case ax::mojom::blink::MoveDirection::kBackward:
          text_boundary = ax::mojom::blink::TextBoundary::kWordStart;
          break;
        case ax::mojom::blink::MoveDirection::kForward:
          text_boundary = ax::mojom::blink::TextBoundary::kWordEnd;
          break;
      }
      break;
    case TextGranularity::kSentence:
      switch (move_direction) {
        case ax::mojom::blink::MoveDirection::kBackward:
          text_boundary = ax::mojom::blink::TextBoundary::kSentenceStart;
          break;
        case ax::mojom::blink::MoveDirection::kForward:
          text_boundary = ax::mojom::blink::TextBoundary::kSentenceEnd;
          break;
      }
      break;
    case TextGranularity::kLine:
      switch (move_direction) {
        case ax::mojom::blink::MoveDirection::kBackward:
          text_boundary = ax::mojom::blink::TextBoundary::kLineStart;
          break;
        case ax::mojom::blink::MoveDirection::kForward:
          text_boundary = ax::mojom::blink::TextBoundary::kLineEnd;
          break;
      }
      break;
    case TextGranularity::kParagraph:
      switch (move_direction) {
        case ax::mojom::blink::MoveDirection::kBackward:
          text_boundary = ax::mojom::blink::TextBoundary::kParagraphStart;
          break;
        case ax::mojom::blink::MoveDirection::kForward:
          text_boundary = ax::mojom::blink::TextBoundary::kParagraphEnd;
          break;
      }
      break;
    case TextGranularity::kSentenceBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kSentenceStartOrEnd;
      break;
    case TextGranularity::kLineBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kLineStartOrEnd;
      break;
    case TextGranularity::kParagraphBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kParagraphStartOrEnd;
      break;
    case TextGranularity::kDocumentBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kWebPage;
      break;
  }

  return BlinkAXEventIntent(command, text_boundary, move_direction);
}

BlinkAXEventIntent BlinkAXEventIntent::FromNewSelection(
    const TextGranularity granularity,
    bool is_base_first,
    const SetSelectionBy set_selection_by) {
  // Unfortunately, when setting a completely new selection, |text_boundary| is
  // not always known, or is hard to compute. For example, if a new selection
  // has been made using the mouse, it would be expensive to compute any
  // meaningful granularity information.
  ax::mojom::blink::TextBoundary text_boundary;
  switch (granularity) {
    case TextGranularity::kCharacter:
      text_boundary = ax::mojom::blink::TextBoundary::kCharacter;
      break;
    case TextGranularity::kWord:
      text_boundary = ax::mojom::blink::TextBoundary::kWordStartOrEnd;
      break;
    case TextGranularity::kSentence:
    case TextGranularity::kSentenceBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kSentenceStartOrEnd;
      break;
    case TextGranularity::kLine:
    case TextGranularity::kLineBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kLineStartOrEnd;
      break;
    case TextGranularity::kParagraph:
    case TextGranularity::kParagraphBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kParagraphStartOrEnd;
      break;
    case TextGranularity::kDocumentBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kWebPage;
      break;
  }

  return BlinkAXEventIntent(
      ax::mojom::blink::Command::kSetSelection, text_boundary,
      is_base_first ? ax::mojom::blink::MoveDirection::kForward
                    : ax::mojom::blink::MoveDirection::kBackward);
}

// Creates an empty (uninitialized) instance.
BlinkAXEventIntent::BlinkAXEventIntent() = default;

BlinkAXEventIntent::BlinkAXEventIntent(
    ax::mojom::blink::Command command,
    ax::mojom::blink::TextBoundary text_boundary,
    ax::mojom::blink::MoveDirection move_direction)
    : intent_(command, text_boundary, move_direction), is_initialized_(true) {}

BlinkAXEventIntent::BlinkAXEventIntent(WTF::HashTableDeletedValueType type)
    : is_initialized_(true), is_deleted_(true) {}

BlinkAXEventIntent::~BlinkAXEventIntent() = default;

BlinkAXEventIntent::BlinkAXEventIntent(const BlinkAXEventIntent& intent) =
    default;

BlinkAXEventIntent& BlinkAXEventIntent::operator=(
    const BlinkAXEventIntent& intent) = default;

bool operator==(const BlinkAXEventIntent& a, const BlinkAXEventIntent& b) {
  return BlinkAXEventIntentHash::GetHash(a) ==
         BlinkAXEventIntentHash::GetHash(b);
}

bool operator!=(const BlinkAXEventIntent& a, const BlinkAXEventIntent& b) {
  return !(a == b);
}

bool BlinkAXEventIntent::IsHashTableDeletedValue() const {
  return is_deleted_;
}

std::string BlinkAXEventIntent::ToString() const {
  if (!is_initialized())
    return "AXEventIntent(uninitialized)";
  if (IsHashTableDeletedValue())
    return "AXEventIntent(is_deleted)";
  return intent().ToString();
}

// static
unsigned int BlinkAXEventIntentHash::GetHash(const BlinkAXEventIntent& key) {
  // If the intent is uninitialized, it is not safe to rely on the memory being
  // initialized to zero, because any uninitialized field that might be
  // accidentally added in the future will produce a potentially non-zero memory
  // value especially in the hard to control "intent_" member.
  if (!key.is_initialized())
    return 0u;
  if (key.IsHashTableDeletedValue())
    return std::numeric_limits<unsigned>::max();

  unsigned hash = 1u;
  WTF::AddIntToHash(hash, static_cast<const unsigned>(key.intent().command));
  WTF::AddIntToHash(hash,
                    static_cast<const unsigned>(key.intent().text_boundary));
  WTF::AddIntToHash(hash,
                    static_cast<const unsigned>(key.intent().move_direction));
  return hash;
}

// static
bool BlinkAXEventIntentHash::Equal(const BlinkAXEventIntent& a,
                                   const BlinkAXEventIntent& b) {
  return a == b;
}

}  // namespace blink
