// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

namespace {

bool ParseTextDirective(const String& fragment,
                        Vector<TextFragmentSelector>* out_selectors) {
  DCHECK(out_selectors);

  size_t start_pos = 0;
  size_t end_pos = 0;
  while (end_pos != kNotFound) {
    if (fragment.Find(kTextFragmentIdentifierPrefix, start_pos) != start_pos) {
      // If this is not a text directive, continue to the next directive
      end_pos = fragment.find('&', start_pos + 1);
      start_pos = end_pos + 1;
      continue;
    }

    start_pos += kTextFragmentIdentifierPrefixStringLength;
    end_pos = fragment.find('&', start_pos);

    String target_text;
    if (end_pos == kNotFound) {
      target_text = fragment.Substring(start_pos);
    } else {
      target_text = fragment.Substring(start_pos, end_pos - start_pos);
      start_pos = end_pos + 1;
    }

    TextFragmentSelector selector = TextFragmentSelector::Create(target_text);
    if (selector.Type() != TextFragmentSelector::kInvalid)
      out_selectors->push_back(selector);
  }

  return out_selectors->size() > 0;
}

bool CheckSecurityRestrictions(LocalFrame& frame,
                               bool same_document_navigation) {
  // This algorithm checks the security restrictions detailed in
  // https://wicg.github.io/ScrollToTextFragment/#should-allow-a-text-fragment
  // TODO(bokan): These are really only relevant for observable actions like
  // scrolling. We should consider allowing highlighting regardless of these
  // conditions. See the TODO in the relevant spec section:
  // https://wicg.github.io/ScrollToTextFragment/#restricting-the-text-fragment

  // History navigation is special because it's considered to be browser
  // initiated even if the navigation originated via use of the history API
  // within the renderer. We avoid creating a text fragment for history
  // navigations since history scroll restoration should take precedence but
  // it'd be bad if we ever got here for a history navigation since the check
  // below would pass even if the user took no action.
  SECURITY_CHECK(frame.Loader().GetDocumentLoader()->GetNavigationType() !=
                 kWebNavigationTypeBackForward);

  // We only allow text fragment anchors for user navigations, e.g. link
  // clicks, omnibox navigations, no script navigations.
  if (!frame.Loader().GetDocumentLoader()->HadTransientActivation() &&
      !frame.Loader().GetDocumentLoader()->IsBrowserInitiated()) {
    return false;
  }

  // Allow same-document navigations only if they are browser initiated, e.g.
  // same-document bookmarks.
  if (same_document_navigation) {
    return frame.Loader()
        .GetDocumentLoader()
        ->LastSameDocumentNavigationWasBrowserInitiated();
  }

  // Allow text fragments on same-origin initiated navigations.
  if (frame.Loader().GetDocumentLoader()->IsSameOriginNavigation())
    return true;

  // Otherwise, for cross origin initiated navigations, we only allow text
  // fragments if the frame is not script accessible by another frame, i.e. no
  // cross origin iframes or window.open.
  if (frame.Tree().Parent() || frame.GetPage()->RelatedPages().size())
    return false;

  return true;
}

}  // namespace

TextFragmentAnchor* TextFragmentAnchor::TryCreateFragmentDirective(
    const KURL& url,
    LocalFrame& frame,
    bool same_document_navigation,
    bool should_scroll) {
  DCHECK(RuntimeEnabledFeatures::TextFragmentIdentifiersEnabled(
      frame.GetDocument()));

  if (!frame.GetDocument()->GetFragmentDirective())
    return nullptr;

  // Avoid invoking the text fragment for history or reload navigations as
  // they'll be clobbered by scroll restoration; this prevents a transient
  // scroll as well as user gesture issues; see https://crbug.com/1042986 for
  // details.
  auto navigation_type =
      frame.Loader().GetDocumentLoader()->GetNavigationType();
  if (navigation_type == kWebNavigationTypeBackForward ||
      navigation_type == kWebNavigationTypeReload) {
    return nullptr;
  }

  if (!CheckSecurityRestrictions(frame, same_document_navigation))
    return nullptr;

  Vector<TextFragmentSelector> selectors;

  if (!ParseTextDirective(frame.GetDocument()->GetFragmentDirective(),
                          &selectors)) {
    UseCounter::Count(frame.GetDocument(),
                      WebFeature::kInvalidFragmentDirective);
    return nullptr;
  }

  return MakeGarbageCollected<TextFragmentAnchor>(selectors, frame,
                                                  should_scroll);
}

TextFragmentAnchor::TextFragmentAnchor(
    const Vector<TextFragmentSelector>& text_fragment_selectors,
    LocalFrame& frame,
    bool should_scroll)
    : frame_(&frame),
      should_scroll_(should_scroll),
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
  if (element_fragment_anchor_) {
    DCHECK(search_finished_);
    // We need to keep this TextFragmentAnchor alive if we're proxying an
    // element fragment anchor.
    return true;
  }

  // If we're done searching, return true if this hasn't been dismissed yet so
  // that this is kept alive.
  if (search_finished_)
    return !dismissed_;

  frame_->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextFragment());

  // TODO(bokan): Once BlockHTMLParserOnStyleSheets is launched, there won't be
  // a way for the user to scroll before we invoke and scroll the anchor. We
  // should confirm if we can remove tracking this after that point or if we
  // need a replacement metric.
  if (user_scrolled_ && !did_scroll_into_view_)
    metrics_->ScrollCancelled();

  first_match_needs_scroll_ = should_scroll_ && !user_scrolled_;

  {
    // FindMatch might cause scrolling and set user_scrolled_ so reset it when
    // it's done.
    base::AutoReset<bool> reset_user_scrolled(&user_scrolled_, user_scrolled_);

    metrics_->ResetMatchCount();
    for (auto& finder : text_fragment_finders_)
      finder.FindMatch(*frame_->GetDocument());
  }

  // Stop searching for matching text once the load event has fired. This may
  // cause ScrollToTextFragment to not work on pages which dynamically load
  // content: http://crbug.com/963045
  if (frame_->GetDocument()->IsLoadCompleted() &&
      beforematch_state_ != kEventQueued)
    DidFinishSearch();

  // We return true to keep this anchor alive as long as we need another invoke,
  // are waiting to be dismissed, or are proxying an element fragment anchor.
  return !search_finished_ || !dismissed_ || element_fragment_anchor_ ||
         beforematch_state_ == kEventQueued;
}

void TextFragmentAnchor::Installed() {}

void TextFragmentAnchor::DidScroll(mojom::blink::ScrollType type) {
  if (!IsExplicitScrollType(type))
    return;

  user_scrolled_ = true;
}

void TextFragmentAnchor::PerformPreRafActions() {
  if (element_fragment_anchor_) {
    element_fragment_anchor_->Installed();
    element_fragment_anchor_->Invoke();
    element_fragment_anchor_->PerformPreRafActions();
    element_fragment_anchor_ = nullptr;
  }
}

void TextFragmentAnchor::Trace(Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(element_fragment_anchor_);
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
           .MarkersIntersectingRange(
               range, DocumentMarker::MarkerTypes::TextFragment())
           .IsEmpty()) {
    return;
  }

  if (beforematch_state_ == kNoMatchFound) {
    Element* enclosing_block =
        EnclosingBlock(range.StartPosition(), kCannotCrossEditingBoundary);
    DCHECK(enclosing_block);
    frame_->GetDocument()->EnqueueAnimationFrameTask(
        WTF::Bind(&TextFragmentAnchor::FireBeforeMatchEvent,
                  WrapPersistent(this), WrapWeakPersistent(enclosing_block)));
    beforematch_state_ = kEventQueued;
    return;
  }
  if (beforematch_state_ == kEventQueued)
    return;
  // TODO(jarhar): Consider what to do based on DOM/style modifications made by
  // the beforematch event here and write tests for it once we decide on a
  // behavior here: https://github.com/WICG/display-locking/issues/150

  bool needs_style_and_layout = false;

  // Apply :target to the first match
  if (!did_find_match_) {
    ApplyTargetToCommonAncestor(range);
    needs_style_and_layout = true;
  }

  // Activate any find-in-page activatable display-locks in the ancestor
  // chain.
  if (DisplayLockUtilities::ActivateFindInPageMatchRangeIfNeeded(range)) {
    // Since activating a lock dirties layout, we need to make sure it's clean
    // before computing the text rect below.
    needs_style_and_layout = true;
    // TODO(crbug.com/1041942): It is possible and likely that activation
    // signal causes script to resize something on the page. This code here
    // should really yield until the next frame to give script an opportunity
    // to run.
  }

  if (needs_style_and_layout) {
    frame_->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kFindInPage);
  }

  metrics_->DidFindMatch(PlainText(range));
  did_find_match_ = true;

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
            bounding_box, ScrollAlignment::CreateScrollIntoViewParams(
                              ScrollAlignment::CenterAlways(),
                              ScrollAlignment::CenterAlways(),
                              mojom::blink::ScrollType::kProgrammatic));
    did_scroll_into_view_ = true;

    if (AXObjectCache* cache = frame_->GetDocument()->ExistingAXObjectCache())
      cache->HandleScrolledToAnchor(&node);

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
  frame_->GetDocument()->Markers().AddTextFragmentMarker(dom_range);
}

void TextFragmentAnchor::DidFindAmbiguousMatch() {
  metrics_->DidFindAmbiguousMatch();
}

void TextFragmentAnchor::DidFinishSearch() {
  DCHECK(!search_finished_);
  search_finished_ = true;

  metrics_->ReportMetrics();

  if (!did_find_match_) {
    dismissed_ = true;

    DCHECK(!element_fragment_anchor_);
    element_fragment_anchor_ = ElementFragmentAnchor::TryCreate(
        frame_->GetDocument()->Url(), *frame_, should_scroll_);
    if (element_fragment_anchor_) {
      // Schedule a frame so we can invoke the element anchor in
      // PerformPreRafActions.
      frame_->GetPage()->GetChromeClient().ScheduleAnimation(frame_->View());
    }
  }
}

bool TextFragmentAnchor::Dismiss() {
  // To decrease the likelihood of the user dismissing the highlight before
  // seeing it, we only dismiss the anchor after search_finished_, at which
  // point we've scrolled it into view or the user has started scrolling the
  // page.
  if (!search_finished_)
    return false;

  if (!did_find_match_ || dismissed_)
    return true;

  DCHECK(!should_scroll_ || did_scroll_into_view_ || user_scrolled_);

  frame_->GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::TextFragment());
  dismissed_ = true;
  metrics_->Dismissed();

  return dismissed_;
}

void TextFragmentAnchor::ApplyTargetToCommonAncestor(
    const EphemeralRangeInFlatTree& range) {
  Node* common_node = range.CommonAncestorContainer();
  while (common_node && common_node->getNodeType() != Node::kElementNode) {
    common_node = common_node->parentNode();
  }

  DCHECK(common_node);
  if (common_node) {
    auto* target = DynamicTo<Element>(common_node);
    frame_->GetDocument()->SetCSSTarget(target);
  }
}

void TextFragmentAnchor::FireBeforeMatchEvent(Element* element) {
  if (RuntimeEnabledFeatures::BeforeMatchEventEnabled())
    element->DispatchEvent(*Event::Create(event_type_names::kBeforematch));
  beforematch_state_ = kFiredEvent;
}

}  // namespace blink
