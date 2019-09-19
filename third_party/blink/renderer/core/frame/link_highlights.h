// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LINK_HIGHLIGHTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LINK_HIGHLIGHTS_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace cc {
class AnimationHost;
}

namespace blink {
class GraphicsContext;
class Page;
class LinkHighlightImpl;
class CompositorAnimationTimeline;
class LocalFrame;
class LayoutObject;

// TODO(wangxianzhu): Since the tap disambiguation feature was removed,
// (http://crrev.com/c/579263), LinkHighlights no longer needs to manage
// multiple link highlights. Rename this class to LinkHighlight and move
// it under core/page, and rename LinkHighlightImpl (core/paint) to
// LinkHighlightPainter. This will be convenient to do when we remove
// GraphicsLayer for CompositeAfterPaint.
class CORE_EXPORT LinkHighlights final
    : public GarbageCollected<LinkHighlights> {
 public:
  explicit LinkHighlights(Page&);
  virtual ~LinkHighlights();

  virtual void Trace(blink::Visitor*);

  void ResetForPageNavigation();

  void SetTapHighlights(HeapVector<Member<Node>>&);

  // Updates geometry on all highlights. See: LinkHighlightImpl::UpdateGeometry.
  void UpdateGeometry();

  void StartHighlightAnimationIfNeeded();

  void AnimationHostInitialized(cc::AnimationHost&);
  void WillCloseAnimationHost();

  bool IsEmpty() const { return link_highlights_.IsEmpty(); }

  bool NeedsHighlightEffect(const LayoutObject& object) const {
    if (link_highlights_.IsEmpty())
      return false;
    return NeedsHighlightEffectInternal(object);
  }

  // For CompositeAfterPaint.
  void Paint(GraphicsContext&) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(LinkHighlightImplTest, verifyWebViewImplIntegration);
  FRIEND_TEST_ALL_PREFIXES(LinkHighlightImplTest, resetDuringNodeRemoval);
  FRIEND_TEST_ALL_PREFIXES(LinkHighlightImplTest, resetLayerTreeView);
  FRIEND_TEST_ALL_PREFIXES(LinkHighlightImplTest, HighlightInvalidation);
  FRIEND_TEST_ALL_PREFIXES(LinkHighlightImplTest, multipleHighlights);
  FRIEND_TEST_ALL_PREFIXES(LinkHighlightImplTest, HighlightLayerEffectNode);
  FRIEND_TEST_ALL_PREFIXES(LinkHighlightImplTest, MultiColumn);
  FRIEND_TEST_ALL_PREFIXES(LinkHighlightSquashingImplTest, SquashingLayer);

  void RemoveAllHighlights();

  LocalFrame* MainFrame() const;

  Page& GetPage() const {
    DCHECK(page_);
    return *page_;
  }

  bool NeedsHighlightEffectInternal(const LayoutObject& object) const;

  Member<Page> page_;
  Vector<std::unique_ptr<LinkHighlightImpl>> link_highlights_;
  cc::AnimationHost* animation_host_ = nullptr;
  std::unique_ptr<CompositorAnimationTimeline> timeline_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LINK_HIGHLIGHTS_H_
