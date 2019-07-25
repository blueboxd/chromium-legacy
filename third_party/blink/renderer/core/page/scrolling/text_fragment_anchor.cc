// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"

#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

namespace {

constexpr char kTextFragmentIdentifierPrefix[] = "targetText=";
constexpr size_t kTextFragmentIdentifierPrefixArrayLength =
    base::size(kTextFragmentIdentifierPrefix);

bool ParseTargetTextIdentifier(const String& fragment,
                               Vector<TextFragmentSelector>* out_selectors) {
  DCHECK(out_selectors);

  size_t start_pos = 0;
  size_t end_pos = 0;
  while (end_pos != kNotFound) {
    if (fragment.Find(kTextFragmentIdentifierPrefix, start_pos) != start_pos)
      return false;

    // The prefix array length includes the \0 string terminator.
    start_pos += kTextFragmentIdentifierPrefixArrayLength - 1;
    end_pos = fragment.find('&', start_pos);

    String target_text;
    if (end_pos == kNotFound) {
      target_text = fragment.Substring(start_pos);
    } else {
      target_text = fragment.Substring(start_pos, end_pos - start_pos);
      start_pos = end_pos + 1;
    }
    out_selectors->push_back(TextFragmentSelector::Create(target_text));
  }

  return true;
}

}  // namespace

TextFragmentAnchor* TextFragmentAnchor::TryCreate(
    const KURL& url,
    LocalFrame& frame,
    bool same_document_navigation) {
  // For security reasons, we only allow text fragments on the main frame of a
  // main window. So no iframes, no window.open. Also only on a full
  // navigation.
  if (frame.Tree().Parent() || frame.DomWindow()->opener() ||
      same_document_navigation)
    return nullptr;

  // For security reasons, we only allow text fragment anchors for user or
  // browser initiated navigations, i.e. no script navigations.
  if (!(frame.Loader().GetDocumentLoader()->HadTransientActivation() ||
        frame.Loader().GetDocumentLoader()->IsBrowserInitiated())) {
    return nullptr;
  }

  Vector<TextFragmentSelector> selectors;

  if (!ParseTargetTextIdentifier(url.FragmentIdentifier(), &selectors))
    return nullptr;

  return MakeGarbageCollected<TextFragmentAnchor>(selectors, frame);
}

TextFragmentAnchor::TextFragmentAnchor(
    const Vector<TextFragmentSelector>& text_fragment_selectors,
    LocalFrame& frame)
    : frame_(&frame),
      metrics_(MakeGarbageCollected<TextFragmentAnchorMetrics>(
          frame_->GetDocument())) {
  DCHECK(!text_fragment_selectors.IsEmpty());
  DCHECK(frame_->View());

  metrics_->DidCreateAnchor(text_fragment_selectors.size());

  text_fragment_finders_.ReserveCapacity(text_fragment_selectors.size());
  for (TextFragmentSelector selector : text_fragment_selectors)
    text_fragment_finders_.emplace_back(*this, selector);
}

bool TextFragmentAnchor::Invoke() {
  if (search_finished_)
    return false;

  frame_->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextMatch());

  if (user_scrolled_ && !did_scroll_into_view_)
    metrics_->ScrollCancelled();

  first_match_needs_scroll_ = !user_scrolled_;

  {
    // FindMatch might cause scrolling and set user_scrolled_ so reset it when
    // it's done.
    base::AutoReset<bool> reset_user_scrolled(&user_scrolled_, user_scrolled_);

    metrics_->ResetMatchCount();
    for (auto& finder : text_fragment_finders_)
      finder.FindMatch(*frame_->GetDocument());
  }

  search_finished_ = frame_->GetDocument()->IsLoadCompleted();

  if (search_finished_)
    metrics_->ReportMetrics();

  return !search_finished_;
}

void TextFragmentAnchor::Installed() {}

void TextFragmentAnchor::DidScroll(ScrollType type) {
  if (!IsExplicitScrollType(type))
    return;

  user_scrolled_ = true;
}

void TextFragmentAnchor::PerformPreRafActions() {}

void TextFragmentAnchor::DidCompleteLoad() {
  if (search_finished_)
    return;

  // If there is a pending layout we'll finish the search from Invoke.
  if (!frame_->View()->NeedsLayout()) {
    metrics_->ReportMetrics();
    search_finished_ = true;
  }
}

void TextFragmentAnchor::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(metrics_);
  FragmentAnchor::Trace(visitor);
}

void TextFragmentAnchor::DidFindMatch(const EphemeralRangeInFlatTree& range) {
  if (search_finished_)
    return;

  // TODO(nburris): Determine what we should do with overlapping text matches.
  // This implementation drops a match if it overlaps a previous match, since
  // overlapping ranges are likely unintentional by the URL creator and could
  // therefore indicate that the page text has changed.
  if (!frame_->GetDocument()
           ->Markers()
           .MarkersIntersectingRange(range,
                                     DocumentMarker::MarkerTypes::TextMatch())
           .IsEmpty()) {
    return;
  }

  metrics_->DidFindMatch();

  if (first_match_needs_scroll_) {
    first_match_needs_scroll_ = false;

    PhysicalRect bounding_box(ComputeTextRect(range));

    // Set the bounding box height to zero because we want to center the top of
    // the text range.
    bounding_box.SetHeight(LayoutUnit());

    DCHECK(range.Nodes().begin() != range.Nodes().end());

    Node& node = *range.Nodes().begin();

    DCHECK(node.GetLayoutObject());

    PhysicalRect scrolled_bounding_box =
        node.GetLayoutObject()->ScrollRectToVisible(
            bounding_box,
            WebScrollIntoViewParams(ScrollAlignment::kAlignCenterAlways,
                                    ScrollAlignment::kAlignCenterAlways,
                                    kProgrammaticScroll));
    did_scroll_into_view_ = true;
    metrics_->DidScroll();

    // We scrolled the text into view if the main document scrolled or the text
    // bounding box changed, i.e. if it was scrolled in a nested scroller.
    // TODO(nburris): The rect returned by ScrollRectToVisible,
    // scrolled_bounding_box, should be in frame coordinates in which case
    // just checking its location would suffice, but there is a bug where it is
    // actually in document coordinates and therefore does not change with a
    // main document scroll.
    if (!frame_->View()->GetScrollableArea()->GetScrollOffset().IsZero() ||
        scrolled_bounding_box.offset != bounding_box.offset) {
      metrics_->DidNonZeroScroll();
    }
  }
  EphemeralRange dom_range =
      EphemeralRange(ToPositionInDOMTree(range.StartPosition()),
                     ToPositionInDOMTree(range.EndPosition()));
  frame_->GetDocument()->Markers().AddTextMatchMarker(
      dom_range, TextMatchMarker::MatchStatus::kInactive);
  frame_->GetEditor().SetMarkedTextMatchesAreHighlighted(true);
}

void TextFragmentAnchor::DidFindAmbiguousMatch() {
  metrics_->DidFindAmbiguousMatch();
}

}  // namespace blink
