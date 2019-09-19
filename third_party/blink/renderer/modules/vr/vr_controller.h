// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/vr/vr_display.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

class NavigatorVR;

class VRController final : public GarbageCollected<VRController>,
                           public device::mojom::blink::VRServiceClient,
                           public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(VRController);
  USING_PRE_FINALIZER(VRController, Dispose);

 public:
  VRController(NavigatorVR*);
  ~VRController() override;

  void GetDisplays(ScriptPromiseResolver*);
  void SetListeningForActivate(bool);

  // VRServiceClient override.
  void OnDeviceChanged() override;

  void FocusChanged();

  void Trace(blink::Visitor*) override;

  mojo::Remote<device::mojom::blink::VRService>& Service() { return service_; }

 private:
  bool ShouldResolveGetDisplays();
  void EnsureDisplay();
  void OnGetDisplays();
  void OnNonImmersiveSessionRequestReturned(
      device::mojom::blink::RequestSessionResultPtr result);

  void OnGetDevicesSuccess(ScriptPromiseResolver*, VRDisplayVector);

  void OnImmersiveDisplayInfoReturned(
      device::mojom::blink::VRDisplayInfoPtr info);

  // ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) override;
  void Dispose();

  void LogGetDisplayResult();

  Member<NavigatorVR> navigator_vr_;
  Member<VRDisplay> display_;

  // True if we don't have an outstanding call to GetImmersiveVRDisplayInfo, so
  // we know what the "latest" immersive display info is.
  bool have_latest_immersive_info_ = false;

  // True if the call to request a non-immersive session has returned.
  bool nonimmersive_session_returned_ = false;
  device::mojom::blink::RequestSessionResultPtr nonimmersive_session_;

  bool has_presentation_capable_display_ = false;
  bool disposed_ = false;
  bool pending_listening_for_activate_ = false;
  bool listening_for_activate_ = false;

  HeapDeque<Member<ScriptPromiseResolver>> pending_promise_resolvers_;
  mojo::Remote<device::mojom::blink::VRService> service_;
  mojo::Receiver<device::mojom::blink::VRServiceClient> receiver_{this};

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(VRController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_CONTROLLER_H_
