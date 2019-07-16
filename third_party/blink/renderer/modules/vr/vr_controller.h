// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/vr/vr_display.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

class NavigatorVR;

class VRController final : public GarbageCollectedFinalized<VRController>,
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

 private:
  void OnGetDisplays();

  void OnGetDevicesSuccess(ScriptPromiseResolver*, VRDisplayVector);

  // Initial callback for requesting the device when VR boots up.
  void OnRequestDeviceReturned(device::mojom::blink::XRDevicePtr);
  // Callback for subsequent request device calls.
  void OnNewDeviceReturned(device::mojom::blink::XRDevicePtr);

  void OnImmersiveDisplayInfoReturned(
      device::mojom::blink::VRDisplayInfoPtr info);

  // ContextLifecycleObserver.
  void ContextDestroyed(ExecutionContext*) override;
  void Dispose();

  void LogGetDisplayResult();

  Member<NavigatorVR> navigator_vr_;
  Member<VRDisplay> display_;

  bool display_synced_;

  bool has_presentation_capable_display_ = false;
  bool has_display_ = false;
  bool pending_listening_for_activate_ = false;
  bool listening_for_activate_ = false;

  HeapDeque<Member<ScriptPromiseResolver>> pending_promise_resolvers_;
  device::mojom::blink::VRServicePtr service_;
  mojo::Binding<device::mojom::blink::VRServiceClient> binding_;

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(VRController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_CONTROLLER_H_
