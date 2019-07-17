// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/vr/navigator_vr.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/vr/vr_controller.h"
#include "third_party/blink/renderer/modules/vr/vr_pose.h"
#include "third_party/blink/renderer/modules/xr/navigator_xr.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

const char kFeaturePolicyBlockedMessage[] =
    "Access to the feature \"vr\" is disallowed by feature policy.";

const char kNotAssociatedWithDocumentMessage[] =
    "The object is no longer associated with a document.";

const char kCannotUseBothNewAndOldAPIMessage[] =
    "Cannot use navigator.getVRDisplays if WebXR is already in use.";

}  // namespace

bool NavigatorVR::HasWebVrBeenUsed(Document& document) {
  if (!document.GetFrame())
    return false;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();

  NavigatorVR* supplement = Supplement<Navigator>::From<NavigatorVR>(navigator);
  if (!supplement) {
    // No supplement means WebVR has not been used.
    return false;
  }

  return NavigatorVR::From(navigator).did_use_webvr_;
}

NavigatorVR* NavigatorVR::From(Document& document) {
  if (!document.GetFrame())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorVR& NavigatorVR::From(Navigator& navigator) {
  NavigatorVR* supplement = Supplement<Navigator>::From<NavigatorVR>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorVR>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* script_state,
                                         Navigator& navigator) {
  if (!navigator.GetFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kNotAssociatedWithDocumentMessage));
  }
  return NavigatorVR::From(navigator).getVRDisplays(script_state);
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* script_state) {
  did_use_webvr_ = true;

  auto* document = GetDocument();
  if (!document) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kNotAssociatedWithDocumentMessage));
  }

  if (!did_log_getVRDisplays_ && document->IsInMainFrame()) {
    did_log_getVRDisplays_ = true;

    ukm::builders::XR_WebXR(document->UkmSourceID())
        .SetDidRequestAvailableDevices(1)
        .Record(document->UkmRecorder());
  }

  if (!document->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebVr,
                                  ReportOptions::kReportOnFailure)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyBlockedMessage));
  }

  // Block developers from using WebVR if they've already used WebXR.
  if (NavigatorXR::HasWebXrBeenUsed(*document)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kCannotUseBothNewAndOldAPIMessage));
  }

  Deprecation::CountDeprecation(*document, WebFeature::kVRGetDisplays);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context->IsSecureContext())
    UseCounter::Count(*document, WebFeature::kVRGetDisplaysInsecureOrigin);

  Platform::Current()->RecordRapporURL("VR.WebVR.GetDisplays", document->Url());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  Controller()->GetDisplays(resolver);

  return promise;
}

VRController* NavigatorVR::Controller() {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  if (!controller_) {
    controller_ = MakeGarbageCollected<VRController>(this);
    controller_->SetListeningForActivate(focused_ && listening_for_activate_);
    controller_->FocusChanged();
  }

  return controller_;
}

Document* NavigatorVR::GetDocument() {
  if (!GetSupplementable() || !GetSupplementable()->GetFrame())
    return nullptr;

  return GetSupplementable()->GetFrame()->GetDocument();
}

void NavigatorVR::Trace(blink::Visitor* visitor) {
  visitor->Trace(controller_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorVR::NavigatorVR(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      FocusChangedObserver(navigator.GetFrame()->GetPage()) {
  navigator.GetFrame()->DomWindow()->RegisterEventListenerObserver(this);
  FocusedFrameChanged();
}

NavigatorVR::~NavigatorVR() = default;

const char NavigatorVR::kSupplementName[] = "NavigatorVR";

void NavigatorVR::EnqueueVREvent(VRDisplayEvent* event) {
  if (!GetSupplementable()->GetFrame())
    return;

  GetSupplementable()->GetFrame()->DomWindow()->EnqueueWindowEvent(
      *event, TaskType::kMiscPlatformAPI);
}

void NavigatorVR::DispatchVREvent(VRDisplayEvent* event) {
  if (!(GetSupplementable()->GetFrame()))
    return;

  LocalDOMWindow* window = GetSupplementable()->GetFrame()->DomWindow();
  DCHECK(window);
  event->SetTarget(window);
  window->DispatchEvent(*event);
}

void NavigatorVR::FocusedFrameChanged() {
  bool focused = IsFrameFocused(GetSupplementable()->GetFrame());
  if (focused == focused_)
    return;
  focused_ = focused;
  if (controller_) {
    controller_->SetListeningForActivate(listening_for_activate_ && focused);
    controller_->FocusChanged();
  }
}

void NavigatorVR::DidAddEventListener(LocalDOMWindow* window,
                                      const AtomicString& event_type) {
  if (event_type == event_type_names::kVrdisplayactivate) {
    listening_for_activate_ = true;
    Controller()->SetListeningForActivate(focused_);
  } else if (event_type == event_type_names::kVrdisplayconnect) {
    // If the page is listening for connection events make sure we've created a
    // controller so that we'll be notified of new devices.
    Controller();
  }
}

void NavigatorVR::DidRemoveEventListener(LocalDOMWindow* window,
                                         const AtomicString& event_type) {
  if (event_type == event_type_names::kVrdisplayactivate &&
      !window->HasEventListeners(event_type_names::kVrdisplayactivate)) {
    listening_for_activate_ = false;
    Controller()->SetListeningForActivate(false);
  }
}

void NavigatorVR::DidRemoveAllEventListeners(LocalDOMWindow* window) {
  if (!controller_)
    return;

  controller_->SetListeningForActivate(false);
  listening_for_activate_ = false;
}

}  // namespace blink
