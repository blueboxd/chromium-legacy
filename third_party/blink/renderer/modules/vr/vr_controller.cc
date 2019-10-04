// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/vr/vr_controller.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/vr/navigator_vr.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

VRController::VRController(NavigatorVR* navigator_vr)
    : ContextLifecycleObserver(navigator_vr->GetDocument()),
      navigator_vr_(navigator_vr),
      feature_handle_for_scheduler_(
          navigator_vr->GetDocument()->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kWebVR,
              {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {
  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      navigator_vr->GetDocument()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  navigator_vr->GetDocument()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
  service_.set_disconnect_handler(
      WTF::Bind(&VRController::Dispose, WrapWeakPersistent(this)));

  service_->SetClient(receiver_.BindNewPipeAndPassRemote(task_runner));

  // Request display info. If we get it, we have a device.
  service_->GetImmersiveVRDisplayInfo(WTF::Bind(
      &VRController::OnImmersiveDisplayInfoReturned, WrapPersistent(this)));

  // Request a non-immersive session immediately as WebVR 1.1 expects to be able
  // to get non-immersive poses as soon as the display is returned.
  device::mojom::blink::XRSessionOptionsPtr options =
      device::mojom::blink::XRSessionOptions::New();
  options->immersive = false;
  options->is_legacy_webvr = true;
  service_->RequestSession(
      std::move(options),
      WTF::Bind(&VRController::OnNonImmersiveSessionRequestReturned,
                WrapPersistent(this)));
}

VRController::~VRController() = default;

bool VRController::ShouldResolveGetDisplays() {
  return have_latest_immersive_info_ && nonimmersive_session_returned_;
}

void VRController::EnsureDisplay() {
  if (!display_) {
    // We have a display for the first time.
    display_ = VRDisplay::Create(navigator_vr_);
    if (pending_listening_for_activate_) {
      SetListeningForActivate(pending_listening_for_activate_);
      pending_listening_for_activate_ = false;
    }
    display_->OnConnected();
    display_->FocusChanged();
  }

  // If we have a non-immersive session, give it to the display so we can
  // satisfy inline animation frame requests.
  if (nonimmersive_session_)
    display_->SetNonImmersiveSession(std::move(nonimmersive_session_));
}

void VRController::GetDisplays(ScriptPromiseResolver* resolver) {
  // If we've previously synced the VRDisplays or no longer have a valid service
  // connection just return the current list. In the case of the service being
  // disconnected this will be an empty array.
  if (!service_ || ShouldResolveGetDisplays()) {
    LogGetDisplayResult();
    HeapVector<Member<VRDisplay>> displays;
    if (display_)
      displays.push_back(display_);
    resolver->Resolve(displays);
    return;
  }

  // Otherwise we're still waiting for the full list of displays to be populated
  // so queue up the promise resolver for resolution when OnGetDisplays is
  // called.
  pending_promise_resolvers_.push_back(resolver);
}

void VRController::SetListeningForActivate(bool listening) {
  if (!service_ || !display_) {
    pending_listening_for_activate_ = listening;
    return;
  }

  if (listening_for_activate_ && listening) {
    // We're already listening so leave things as is.
    return;
  }

  listening_for_activate_ = listening;

  if (listening)
    service_->SetListeningForActivate(display_->GetDisplayClient());
  else
    service_->SetListeningForActivate(mojo::NullRemote());
}

void VRController::OnDeviceChanged() {
  if (!have_latest_immersive_info_) {
    // We're already underway checking if there is a device.
    return;
  }

  have_latest_immersive_info_ = false;

  // Get updated display info.
  service_->GetImmersiveVRDisplayInfo(WTF::Bind(
      &VRController::OnImmersiveDisplayInfoReturned, WrapPersistent(this)));
}

void VRController::FocusChanged() {
  if (display_)
    display_->FocusChanged();
}

void VRController::OnImmersiveDisplayInfoReturned(
    device::mojom::blink::VRDisplayInfoPtr info) {
  if (disposed_) {
    return;
  }

  if (info) {
    has_presentation_capable_display_ = info->capabilities->can_present;
  } else {
    has_presentation_capable_display_ = false;
  }

  if (info) {
    EnsureDisplay();
    display_->OnChanged(std::move(info), true /* is_immersive */);
  }

  // We know whether there is a display at this point.
  have_latest_immersive_info_ = true;

  if (ShouldResolveGetDisplays())
    OnGetDisplays();
}

void VRController::LogGetDisplayResult() {
  Document* doc = navigator_vr_->GetDocument();
  if (display_ && doc && doc->IsInMainFrame()) {
    ukm::builders::XR_WebXR ukm_builder(doc->UkmSourceID());
    ukm_builder.SetReturnedDevice(1);
    if (has_presentation_capable_display_) {
      ukm_builder.SetReturnedPresentationCapableDevice(1);
    }
    ukm_builder.Record(doc->UkmRecorder());
  }
}

void VRController::OnGetDisplays() {
  while (!pending_promise_resolvers_.IsEmpty()) {
    LogGetDisplayResult();

    HeapVector<Member<VRDisplay>> displays;
    if (display_)
      displays.push_back(display_);

    auto promise_resolver = pending_promise_resolvers_.TakeFirst();
    OnGetDevicesSuccess(promise_resolver, displays);
  }
}

void VRController::OnNonImmersiveSessionRequestReturned(
    device::mojom::blink::RequestSessionResultPtr result) {
  if (disposed_) {
    return;
  }

  nonimmersive_session_returned_ = true;
  nonimmersive_session_ = std::move(result);

  // If we support non-immersive, we have a display.
  if (nonimmersive_session_->is_session())
    EnsureDisplay();

  if (ShouldResolveGetDisplays())
    OnGetDisplays();
}

void VRController::OnGetDevicesSuccess(ScriptPromiseResolver* resolver,
                                       VRDisplayVector displays) {
  bool display_supports_presentation = false;
  for (auto display : displays) {
    if (display->capabilities()->canPresent()) {
      display_supports_presentation = true;
    }
  }

  if (display_supports_presentation) {
    ExecutionContext* execution_context =
        ExecutionContext::From(resolver->GetScriptState());
    UseCounter::Count(execution_context,
                      WebFeature::kVRGetDisplaysSupportsPresent);
  }

  resolver->Resolve(displays);
}

void VRController::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void VRController::Dispose() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  service_.reset();
  receiver_.reset();

  // Shutdown all displays' message pipe
  if (display_) {
    display_->Dispose();
    display_ = nullptr;
  }

  disposed_ = true;

  // Ensure that any outstanding getDisplays promises are resolved.
  OnGetDisplays();
}

void VRController::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_vr_);
  visitor->Trace(display_);
  visitor->Trace(pending_promise_resolvers_);

  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
