// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_element.h"

#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_media_event_listener.h"

namespace blink {

namespace {

// Amount of time that media controls are visible.
constexpr base::TimeDelta kTimeToHideControl =
    base::TimeDelta::FromMilliseconds(3000);

const char kTransparentCSSClass[] = "transparent";
const char kTransparentImmediateCSSClass[] = "transparent-immediate";

}  // namespace

MediaControlsTouchlessElement::MediaControlsTouchlessElement(
    MediaControlsTouchlessImpl& media_controls)
    : HTMLDivElement(media_controls.GetDocument()),
      media_controls_(media_controls) {
  media_controls_->MediaEventListener().AddObserver(this);
}

HTMLMediaElement& MediaControlsTouchlessElement::MediaElement() const {
  return media_controls_->MediaElement();
}

void MediaControlsTouchlessElement::MakeOpaque(bool should_hide) {
  EnsureHideControlTimer();

  removeAttribute("class");

  if (hide_control_timer_->IsActive())
    StopHideControlTimer();

  if (should_hide)
    StartHideControlTimer();
}

void MediaControlsTouchlessElement::MakeTransparent(bool hide_immediate) {
  classList().Add(hide_immediate ? kTransparentImmediateCSSClass
                                 : kTransparentCSSClass);
}

void MediaControlsTouchlessElement::EnsureHideControlTimer() {
  if (!hide_control_timer_) {
    hide_control_timer_ =
        std::make_unique<TaskRunnerTimer<MediaControlsTouchlessElement>>(
            MediaElement().GetDocument().GetTaskRunner(
                TaskType::kInternalMedia),
            this, &MediaControlsTouchlessElement::HideControlTimerFired);
  }
}

void MediaControlsTouchlessElement::HideControlTimerFired(TimerBase*) {
  MakeTransparent();
}

void MediaControlsTouchlessElement::StartHideControlTimer() {
  hide_control_timer_->StartOneShot(kTimeToHideControl, FROM_HERE);
}

void MediaControlsTouchlessElement::StopHideControlTimer() {
  hide_control_timer_->Stop();
}

void MediaControlsTouchlessElement::Trace(blink::Visitor* visitor) {
  HTMLDivElement::Trace(visitor);
  visitor->Trace(media_controls_);
}

}  // namespace blink
