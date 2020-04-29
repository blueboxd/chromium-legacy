// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/presentation/presentation_receiver.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Presentation::Presentation(LocalDOMWindow* window)
    : ExecutionContextClient(window) {}

// static
Presentation* Presentation::Create(LocalDOMWindow* window) {
  DCHECK(window);
  Presentation* presentation = MakeGarbageCollected<Presentation>(window);
  PresentationController* controller = PresentationController::From(*window);
  DCHECK(controller);
  controller->SetPresentation(presentation);
  return presentation;
}

void Presentation::Trace(Visitor* visitor) {
  visitor->Trace(default_request_);
  visitor->Trace(receiver_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

PresentationRequest* Presentation::defaultRequest() const {
  return default_request_;
}

void Presentation::setDefaultRequest(PresentationRequest* request) {
  default_request_ = request;

  if (!GetExecutionContext())
    return;

  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  controller->GetPresentationService()->SetDefaultPresentationUrls(
      request ? request->Urls() : WTF::Vector<KURL>());
}

PresentationReceiver* Presentation::receiver() {
  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (!window ||
      !window->GetFrame()->GetSettings()->GetPresentationReceiver()) {
    return nullptr;
  }

  if (!receiver_)
    receiver_ = MakeGarbageCollected<PresentationReceiver>(window);

  return receiver_;
}

}  // namespace blink
