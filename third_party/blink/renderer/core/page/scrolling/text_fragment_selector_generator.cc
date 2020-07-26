// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector_generator.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"

namespace blink {

constexpr int kExactTextMaxChars = 300;
constexpr int kNoContextMinChars = 20;

TextFragmentSelectorGenerator::TextFragmentSelectorGenerator(
    LocalFrame* frame,
    base::OnceCallback<void(const TextFragmentSelector&)> callback)
    : frame_(frame), callback_(std::move(callback)) {}

// TextFragmentSelectorGenerator is responsible for generating text fragment
// selectors that have a unique match.
void TextFragmentSelectorGenerator::GenerateSelector(
    const EphemeralRangeInFlatTree& selection_range) {
  const TextFragmentSelector kInvalidSelector(
      TextFragmentSelector::SelectorType::kInvalid);

  Node& start_first_block_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(
          *selection_range.StartPosition().AnchorNode());
  Node& end_first_block_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(
          *selection_range.EndPosition().AnchorNode());

  if (!start_first_block_ancestor.isSameNode(&end_first_block_ancestor))
    std::move(callback_).Run(kInvalidSelector);

  // TODO(gayane): If same node, need to check if start and end are interrupted
  // by a block. Example: <div>start of the selection <div> sub block </div>end
  // of the selection</div>.

  // TODO(gayane): Move selection start and end to contain full words.

  String selected_text = PlainText(selection_range);

  if (selected_text.length() < kNoContextMinChars ||
      selected_text.length() > kExactTextMaxChars)
    std::move(callback_).Run(kInvalidSelector);

  selector_ = std::make_unique<TextFragmentSelector>(
      TextFragmentSelector::SelectorType::kExact, selected_text, "", "", "");
  TextFragmentFinder finder(*this, *selector_);
  finder.FindMatch(*frame_->GetDocument());
}

void TextFragmentSelectorGenerator::DidFindMatch(
    const EphemeralRangeInFlatTree& match,
    const TextFragmentAnchorMetrics::Match match_metrics,
    bool is_unique) {
  if (is_unique) {
    std::move(callback_).Run(*selector_);
  } else {
    // TODO(gayane): Should add more range and/or context.
    std::move(callback_).Run(
        TextFragmentSelector(TextFragmentSelector::SelectorType::kInvalid));
  }
}

void TextFragmentSelectorGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

}  // namespace blink
