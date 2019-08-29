// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_bottom_container_element.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_timeline_element.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

MediaControlsTouchlessBottomContainerElement::
    MediaControlsTouchlessBottomContainerElement(
        MediaControlsTouchlessImpl& media_controls)
    : MediaControlsTouchlessElement(media_controls) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-touchless-bottom-container"));

  time_display_element_ =
      MakeGarbageCollected<MediaControlsTouchlessTimeDisplayElement>(
          media_controls);
  timeline_element_ =
      MakeGarbageCollected<MediaControlsTouchlessTimelineElement>(
          media_controls);

  ParserAppendChild(time_display_element_);
  ParserAppendChild(timeline_element_);

  event_listener_ =
      MakeGarbageCollected<MediaControlsSharedHelpers::TransitionEventListener>(
          this,
          WTF::BindRepeating(&MediaControlsTouchlessBottomContainerElement::
                                 HandleTransitionEndEvent,
                             WrapWeakPersistent(this)));
  event_listener_->Attach();
}

LayoutObject*
MediaControlsTouchlessBottomContainerElement::TimelineLayoutObject() {
  return timeline_element_->GetLayoutObject();
}

LayoutObject*
MediaControlsTouchlessBottomContainerElement::TimeDisplayLayoutObject() {
  return time_display_element_->GetLayoutObject();
}

void MediaControlsTouchlessBottomContainerElement::MakeOpaque(
    bool should_hide) {
  SetDisplayed(true);
  MediaElement().MediaControlsDidBecomeVisible();
  MediaControlsTouchlessElement::MakeOpaque(should_hide);
}

void MediaControlsTouchlessBottomContainerElement::HandleTransitionEndEvent() {
  SetDisplayed(false);
}

void MediaControlsTouchlessBottomContainerElement::SetDisplayed(
    bool displayed) {
  if (displayed)
    RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
  else
    SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
}

void MediaControlsTouchlessBottomContainerElement::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(timeline_element_);
  visitor->Trace(time_display_element_);
  visitor->Trace(event_listener_);
  MediaControlsTouchlessElement::Trace(visitor);
}

}  // namespace blink
