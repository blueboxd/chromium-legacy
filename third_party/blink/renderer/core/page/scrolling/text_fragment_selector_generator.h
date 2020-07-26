// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT TextFragmentSelectorGenerator final
    : public GarbageCollected<TextFragmentSelectorGenerator>,
      public TextFragmentFinder::Client {
 public:
  TextFragmentSelectorGenerator(
      LocalFrame* frame,
      base::OnceCallback<void(const TextFragmentSelector&)> callback);

  void GenerateSelector(const EphemeralRangeInFlatTree& selection_range);

  // TextFragmentFinder::Client interface
  void DidFindMatch(const EphemeralRangeInFlatTree& match,
                    const TextFragmentAnchorMetrics::Match match_metrics,
                    bool is_unique) override;

  void Trace(Visitor*) const;

 private:
  Member<LocalFrame> frame_;
  base::OnceCallback<void(const TextFragmentSelector&)> callback_;
  std::unique_ptr<TextFragmentSelector> selector_;

  DISALLOW_COPY_AND_ASSIGN(TextFragmentSelectorGenerator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
