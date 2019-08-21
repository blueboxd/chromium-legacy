// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/link_highlights.h"

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/link_highlight_impl.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"

namespace blink {

LinkHighlights::LinkHighlights(Page& owner) : page_(&owner) {}

LinkHighlights::~LinkHighlights() {
  RemoveAllHighlights();
}

void LinkHighlights::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
}

void LinkHighlights::RemoveAllHighlights() {
  for (auto& highlight : link_highlights_) {
    if (timeline_)
      timeline_->AnimationDestroyed(*highlight);
    if (auto* node = highlight->GetNode()) {
      if (auto* layout_object = node->GetLayoutObject())
        layout_object->SetNeedsPaintPropertyUpdate();
    }
  }
  link_highlights_.clear();
}

void LinkHighlights::ResetForPageNavigation() {
  RemoveAllHighlights();
}

void LinkHighlights::SetTapHighlights(
    HeapVector<Member<Node>>& highlight_nodes) {
  // Always clear any existing highlight when this is invoked, even if we
  // don't get a new target to highlight.
  RemoveAllHighlights();

  for (wtf_size_t i = 0; i < highlight_nodes.size(); ++i) {
    Node* node = highlight_nodes[i];

    if (!node || !node->GetLayoutObject())
      continue;

    Color highlight_color =
        node->GetLayoutObject()->StyleRef().TapHighlightColor();
    // Safari documentation for -webkit-tap-highlight-color says if the
    // specified color has 0 alpha, then tap highlighting is disabled.
    // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
    if (!highlight_color.Alpha())
      continue;

    link_highlights_.push_back(std::make_unique<LinkHighlightImpl>(node));
    if (timeline_)
      timeline_->AnimationAttached(*link_highlights_.back());
    node->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
  }
}

void LinkHighlights::UpdateGeometry() {
  for (auto& highlight : link_highlights_)
    highlight->UpdateGeometry();
}

LocalFrame* LinkHighlights::MainFrame() const {
  return GetPage().MainFrame() && GetPage().MainFrame()->IsLocalFrame()
             ? GetPage().DeprecatedLocalMainFrame()
             : nullptr;
}

void LinkHighlights::StartHighlightAnimationIfNeeded() {
  for (auto& highlight : link_highlights_)
    highlight->StartHighlightAnimationIfNeeded();

  if (auto* local_frame = MainFrame())
    GetPage().GetChromeClient().ScheduleAnimation(local_frame->View());
}

void LinkHighlights::AnimationHostInitialized(
    cc::AnimationHost& animation_host) {
  animation_host_ = &animation_host;
  if (Platform::Current()->IsThreadedAnimationEnabled()) {
    timeline_ = std::make_unique<CompositorAnimationTimeline>();
    animation_host_->AddAnimationTimeline(timeline_->GetAnimationTimeline());
  }
}

void LinkHighlights::WillCloseAnimationHost() {
  RemoveAllHighlights();
  if (timeline_) {
    animation_host_->RemoveAnimationTimeline(timeline_->GetAnimationTimeline());
    timeline_.reset();
  }
  animation_host_ = nullptr;
}

bool LinkHighlights::NeedsHighlightEffectInternal(
    const LayoutObject& object) const {
  for (auto& highlight : link_highlights_) {
    if (auto* node = highlight->GetNode()) {
      if (node->GetLayoutObject() == &object)
        return true;
    }
  }
  return false;
}

void LinkHighlights::Paint(GraphicsContext& context) const {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  for (const auto& highlight : link_highlights_)
    highlight->Paint(context);
}

}  // namespace blink
