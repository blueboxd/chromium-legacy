// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_custom_controls_fullscreen_detector.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using blink::WebFullscreenVideoStatus;

namespace {

constexpr float kMostlyFillViewportIntersectionThreshold = 0.85f;

}  // anonymous namespace

MediaCustomControlsFullscreenDetector::MediaCustomControlsFullscreenDetector(
    HTMLVideoElement& video)
    : video_element_(video), viewport_intersection_observer_(nullptr) {
  if (VideoElement().isConnected())
    Attach();
}

void MediaCustomControlsFullscreenDetector::Attach() {
  VideoElement().addEventListener(event_type_names::kLoadedmetadata, this,
                                  true);
  VideoElement().GetDocument().addEventListener(
      event_type_names::kWebkitfullscreenchange, this, true);
  VideoElement().GetDocument().addEventListener(
      event_type_names::kFullscreenchange, this, true);
  constexpr float threshold = kMostlyFillViewportIntersectionThreshold;
  viewport_intersection_observer_ = IntersectionObserver::Create(
      {}, {threshold}, &(video_element_->GetDocument()),
      WTF::BindRepeating(
          &MediaCustomControlsFullscreenDetector::OnIntersectionChanged,
          WrapWeakPersistent(this)),
      IntersectionObserver::kDeliverDuringPostLifecycleSteps,
      IntersectionObserver::kFractionOfRoot, 0, false, true);
  viewport_intersection_observer_->observe(&VideoElement());
}

void MediaCustomControlsFullscreenDetector::Detach() {
  if (viewport_intersection_observer_) {
    viewport_intersection_observer_->disconnect();
    viewport_intersection_observer_ = nullptr;
  }
  VideoElement().removeEventListener(event_type_names::kLoadedmetadata, this,
                                     true);
  VideoElement().GetDocument().removeEventListener(
      event_type_names::kWebkitfullscreenchange, this, true);
  VideoElement().GetDocument().removeEventListener(
      event_type_names::kFullscreenchange, this, true);
  VideoElement().SetIsEffectivelyFullscreen(
      WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
}

void MediaCustomControlsFullscreenDetector::Invoke(ExecutionContext* context,
                                                   Event* event) {
  DCHECK(event->type() == event_type_names::kLoadedmetadata ||
         event->type() == event_type_names::kWebkitfullscreenchange ||
         event->type() == event_type_names::kFullscreenchange);

  // Video is not loaded yet.
  if (VideoElement().getReadyState() < HTMLMediaElement::kHaveMetadata)
    return;

  TriggerObservation();
}

void MediaCustomControlsFullscreenDetector::ContextDestroyed() {
  Detach();
}

void MediaCustomControlsFullscreenDetector::OnIntersectionChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  if (!viewport_intersection_observer_ || entries.size() == 0)
    return;

  const bool is_mostly_filling_viewport =
      entries.back()->intersectionRatio() >=
      kMostlyFillViewportIntersectionThreshold;
  VideoElement().SetIsDominantVisibleContent(is_mostly_filling_viewport);

  if (!is_mostly_filling_viewport || !IsVideoOrParentFullscreen()) {
    VideoElement().SetIsEffectivelyFullscreen(
        WebFullscreenVideoStatus::kNotEffectivelyFullscreen);
    return;
  }

  // Picture-in-Picture can be disabled by the website when the API is enabled.
  bool picture_in_picture_allowed =
      !RuntimeEnabledFeatures::PictureInPictureEnabled() &&
      !VideoElement().FastHasAttribute(
          html_names::kDisablepictureinpictureAttr);

  if (picture_in_picture_allowed) {
    VideoElement().SetIsEffectivelyFullscreen(
        WebFullscreenVideoStatus::kFullscreenAndPictureInPictureEnabled);
  } else {
    VideoElement().SetIsEffectivelyFullscreen(
        WebFullscreenVideoStatus::kFullscreenAndPictureInPictureDisabled);
  }
}

void MediaCustomControlsFullscreenDetector::TriggerObservation() {
  if (!viewport_intersection_observer_)
    return;

  // Removing and re-adding the observable element is just a way to
  // trigger the observation callback and reevaluate the intersection ratio.
  viewport_intersection_observer_->unobserve(&VideoElement());
  viewport_intersection_observer_->observe(&VideoElement());
}

bool MediaCustomControlsFullscreenDetector::IsVideoOrParentFullscreen() {
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(VideoElement().GetDocument());
  if (!fullscreen_element)
    return false;

  return fullscreen_element->contains(&VideoElement());
}

void MediaCustomControlsFullscreenDetector::Trace(Visitor* visitor) {
  NativeEventListener::Trace(visitor);
  visitor->Trace(video_element_);
  visitor->Trace(viewport_intersection_observer_);
}

}  // namespace blink
