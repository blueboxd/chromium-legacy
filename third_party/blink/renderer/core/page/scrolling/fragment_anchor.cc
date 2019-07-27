// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

FragmentAnchor* FragmentAnchor::TryCreate(const KURL& url,
                                          LocalFrame& frame,
                                          bool same_document_navigation) {
  DCHECK(frame.GetDocument());

  FragmentAnchor* anchor = nullptr;
  const bool text_fragment_identifiers_enabled =
      RuntimeEnabledFeatures::TextFragmentIdentifiersEnabled(
          frame.GetDocument());

  // The text fragment anchor will be created if we successfully parsed the
  // targetText but we only do the text matching later on.
  bool text_fragment_anchor_created = false;
  if (text_fragment_identifiers_enabled) {
    anchor = TextFragmentAnchor::TryCreateFragmentDirective(
        url, frame, same_document_navigation);
    text_fragment_anchor_created = anchor;
  }

  // Count how often we see a # in the fragment (i.e. after the # delimiting
  // the hash). We do this after trying to find a ##targetText so that we don't
  // pollute this metric with our own feature since we're trying to determine
  // how prevalent ## is. If a ##targetText is found, it'll strip off the ##
  // and any text following it.
  if (url.HasFragmentIdentifier()) {
    size_t hash_pos = url.FragmentIdentifier().Find("#");
    if (hash_pos != kNotFound)
      UseCounter::Count(frame.GetDocument(), WebFeature::kFragmentDoubleHash);
  }

  bool element_id_anchor_found = false;
  if (!anchor) {
    anchor = ElementFragmentAnchor::TryCreate(url, frame);
    element_id_anchor_found = anchor;
  }

  if (text_fragment_identifiers_enabled) {
    FragmentAnchor* text_anchor =
        TextFragmentAnchor::TryCreate(url, frame, same_document_navigation);
    text_fragment_anchor_created |= static_cast<bool>(text_anchor);
    // We parse the anchor to determine if we should report the UMA metric
    // below but we should only use it if we didn't find an element-id based
    // fragment.
    if (!anchor)
      anchor = text_anchor;
  }

  // Track how often we have a element fragment that we can't find. Only track
  // if we didn't match a text fragment since we expect those would inflate the
  // "failed" case.
  if (frame.GetDocument()->IsHTMLDocument() && url.HasFragmentIdentifier() &&
      !text_fragment_anchor_created) {
    UMA_HISTOGRAM_BOOLEAN("TextFragmentAnchor.ElementIdFragmentFound",
                          element_id_anchor_found);
  }

  return anchor;
}

}  // namespace blink
