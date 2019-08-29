// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_media_event_listener.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_media_event_listener_observer.h"

namespace blink {

MediaControlsTouchlessMediaEventListener::
    MediaControlsTouchlessMediaEventListener(HTMLMediaElement& media_element)
    : media_element_(media_element) {
  if (media_element.isConnected())
    Attach();
}

void MediaControlsTouchlessMediaEventListener::AddObserver(
    MediaControlsTouchlessMediaEventListenerObserver* observer) {
  observers_.insert(observer);
}

void MediaControlsTouchlessMediaEventListener::RemoveObserver(
    MediaControlsTouchlessMediaEventListenerObserver* observer) {
  observers_.erase(observer);
}

void MediaControlsTouchlessMediaEventListener::Attach() {
  DCHECK(media_element_->isConnected());

  media_element_->addEventListener(event_type_names::kFocusin, this, false);
  media_element_->addEventListener(event_type_names::kTimeupdate, this, false);
  media_element_->addEventListener(event_type_names::kDurationchange, this,
                                   false);
  media_element_->addEventListener(event_type_names::kSeeking, this, false);
  media_element_->addEventListener(event_type_names::kProgress, this, false);

  media_element_->addEventListener(event_type_names::kPlay, this, false);
  media_element_->addEventListener(event_type_names::kPause, this, false);

  media_element_->addEventListener(event_type_names::kError, this, false);
  media_element_->addEventListener(event_type_names::kLoadedmetadata, this,
                                   false);

  media_element_->addEventListener(event_type_names::kKeypress, this, false);
  media_element_->addEventListener(event_type_names::kKeydown, this, false);
  media_element_->addEventListener(event_type_names::kKeyup, this, false);

  media_element_->addEventListener(event_type_names::kWebkitfullscreenchange,
                                   this, false);
  media_element_->GetDocument().addEventListener(
      event_type_names::kFullscreenchange, this, false);
}

void MediaControlsTouchlessMediaEventListener::Detach() {
  DCHECK(!media_element_->isConnected());
}

void MediaControlsTouchlessMediaEventListener::Invoke(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->type() == event_type_names::kFocusin) {
    for (auto& observer : observers_)
      observer->OnFocusIn();
    return;
  }
  if (event->type() == event_type_names::kTimeupdate) {
    for (auto& observer : observers_)
      observer->OnTimeUpdate();
    return;
  }
  if (event->type() == event_type_names::kDurationchange) {
    for (auto& observer : observers_)
      observer->OnDurationChange();
    return;
  }
  if (event->type() == event_type_names::kSeeking) {
    for (auto& observer : observers_)
      observer->OnSeeking();
    return;
  }
  if (event->type() == event_type_names::kProgress) {
    for (auto& observer : observers_)
      observer->OnLoadingProgress();
    return;
  }
  if (event->type() == event_type_names::kPlay) {
    for (auto& observer : observers_)
      observer->OnPlay();
    return;
  }
  if (event->type() == event_type_names::kPause) {
    for (auto& observer : observers_)
      observer->OnPause();
    return;
  }
  if (event->type() == event_type_names::kError) {
    for (auto& observer : observers_)
      observer->OnError();
    return;
  }
  if (event->type() == event_type_names::kLoadedmetadata) {
    for (auto& observer : observers_)
      observer->OnLoadedMetadata();
    return;
  }
  if (event->type() == event_type_names::kKeypress) {
    for (auto& observer : observers_)
      observer->OnKeyPress(ToKeyboardEvent(event));
    return;
  }
  if (event->type() == event_type_names::kKeydown) {
    for (auto& observer : observers_)
      observer->OnKeyDown(ToKeyboardEvent(event));
    return;
  }
  if (event->type() == event_type_names::kKeyup) {
    for (auto& observer : observers_)
      observer->OnKeyUp(ToKeyboardEvent(event));
    return;
  }
  if (event->type() == event_type_names::kFullscreenchange ||
      event->type() == event_type_names::kWebkitfullscreenchange) {
    if (media_element_->IsFullscreen()) {
      for (auto& observer : observers_)
        observer->OnEnterFullscreen();
    } else {
      for (auto& observer : observers_)
        observer->OnExitFullscreen();
    }
  }
}

void MediaControlsTouchlessMediaEventListener::Trace(blink::Visitor* visitor) {
  NativeEventListener::Trace(visitor);
  visitor->Trace(media_element_);
  visitor->Trace(observers_);
}

}  // namespace blink
