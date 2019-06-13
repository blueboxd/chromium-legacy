// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_request_callback_collection.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_array.h"
#include "third_party/blink/renderer/platform/geometry/double_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"

namespace blink {

class Element;
class ExceptionState;
class HTMLCanvasElement;
class ResizeObserver;
class ScriptPromiseResolver;
class V8XRFrameRequestCallback;
class XR;
class XRCanvasInputProvider;
class XRSpace;
class XRInputSourceEvent;
class XRRay;
class XRReferenceSpace;
class XRRenderState;
class XRRenderStateInit;
class XRViewData;
class XRWorldInformation;
class XRWorldTrackingState;
class XRWorldTrackingStateInit;

class XRSession final : public EventTargetWithInlineData,
                        public device::mojom::blink::XRSessionClient,
                        public ActiveScriptWrappable<XRSession> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XRSession);

 public:
  enum SessionMode { kModeInline = 0, kModeImmersiveVR, kModeImmersiveAR };

  enum EnvironmentBlendMode {
    kBlendModeOpaque = 0,
    kBlendModeAdditive,
    kBlendModeAlphaBlend
  };

  XRSession(XR*,
            device::mojom::blink::XRSessionClientRequest client_request,
            SessionMode mode,
            EnvironmentBlendMode environment_blend_mode,
            bool sensorless_session);
  ~XRSession() override = default;

  XR* xr() const { return xr_; }
  const String& environmentBlendMode() const { return blend_mode_string_; }
  XRRenderState* renderState() const { return render_state_; }
  XRWorldTrackingState* worldTrackingState() { return world_tracking_state_; }
  XRSpace* viewerSpace() const;

  bool immersive() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(blur, kBlur)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(focus, kFocus)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(end, kEnd)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(select, kSelect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(inputsourceschange, kInputsourceschange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart, kSelectstart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectend, kSelectend)

  void updateRenderState(XRRenderStateInit*, ExceptionState&);
  void updateWorldTrackingState(
      XRWorldTrackingStateInit* worldTrackingStateInit,
      ExceptionState& exception_state);
  ScriptPromise requestReferenceSpace(ScriptState*, const String&);

  int requestAnimationFrame(V8XRFrameRequestCallback*);
  void cancelAnimationFrame(int id);

  XRInputSourceArray* inputSources() const;

  ScriptPromise requestHitTest(ScriptState* script_state,
                               XRRay* ray,
                               XRSpace* space);

  // Called by JavaScript to manually end the session.
  ScriptPromise end(ScriptState*);

  bool ended() { return ended_; }

  // Called when the session is ended, either via calling the "end" function or
  // when the presentation service connection is closed.
  void ForceEnd();

  // Describes the scalar to be applied to the default framebuffer dimensions
  // which gives 1:1 pixel ratio at the center of the user's view.
  double NativeFramebufferScale() const;

  // Describes the recommended dimensions of layer framebuffers. Should be a
  // value that provides a good balance between quality and performance.
  DoubleSize DefaultFramebufferSize() const;

  // Reports the size of the output canvas, if one is available. If not
  // reports (0, 0);
  DoubleSize OutputCanvasSize() const;
  void DetachOutputCanvas(HTMLCanvasElement* output_canvas);

  void LogGetPose() const;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void OnFocusChanged();
  void OnFrame(double timestamp,
               std::unique_ptr<TransformationMatrix>,
               const base::Optional<gpu::MailboxHolder>& output_mailbox_holder,
               const device::mojom::blink::XRPlaneDetectionDataPtr&
                   detected_planes_data);

  void OnInputStateChange(
      int16_t frame_id,
      const WTF::Vector<device::mojom::blink::XRInputSourceStatePtr>&);

  WTF::Vector<XRViewData>& views();

  void AddTransientInputSource(XRInputSource*);
  void RemoveTransientInputSource(XRInputSource*);

  void OnSelectStart(XRInputSource*);
  void OnSelectEnd(XRInputSource*);
  void OnSelect(XRInputSource*);

  void OnPoseReset();

  const device::mojom::blink::VRDisplayInfoPtr& GetVRDisplayInfo() const {
    return display_info_;
  }

  // TODO(crbug.com/969131): Update the mojom to deliver this per-frame.
  bool EmulatedPosition() const {
    if (display_info_) {
      return !display_info_->capabilities->hasPosition;
    }

    // If we don't have display info then we should be using the identity
    // reference space, which by definition will be emulating the position.
    return true;
  }

  // Immersive sessions currently use two views for VR, and only a single view
  // for smartphone immersive AR mode. Convention is that we use the left eye
  // if there's only a single view.
  bool StereoscopicViews() { return display_info_ && display_info_->rightEye; }

  void UpdateEyeParameters(
      const device::mojom::blink::VREyeParametersPtr& left_eye,
      const device::mojom::blink::VREyeParametersPtr& right_eye);
  void UpdateStageParameters(
      const device::mojom::blink::VRStageParametersPtr& stage_parameters);
  bool External() const { return is_external_; }
  // Incremented every time display_info_ is changed, so that other objects that
  // depend on it can know when they need to update.
  unsigned int DisplayInfoPtrId() const { return display_info_id_; }
  unsigned int StageParametersId() const { return stage_parameters_id_; }

  void SetXRDisplayInfo(device::mojom::blink::VRDisplayInfoPtr display_info);

  void Trace(blink::Visitor*) override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

 private:
  class XRSessionResizeObserverDelegate;

  XRFrame* CreatePresentationFrame();
  void UpdateCanvasDimensions(Element*);
  void ApplyPendingRenderState();

  void UpdateSelectState(XRInputSource*,
                         const device::mojom::blink::XRInputSourceStatePtr&);
  void UpdateSelectStateOnRemoval(XRInputSource*);
  XRInputSourceEvent* CreateInputSourceEvent(const AtomicString&,
                                             XRInputSource*);

  // XRSessionClient
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr) override;
  void OnExitPresent() override;
  void OnFocus() override;
  void OnBlur() override;

  bool HasAppropriateFocus();

  void OnHitTestResults(
      ScriptPromiseResolver* resolver,
      base::Optional<WTF::Vector<device::mojom::blink::XRHitResultPtr>>
          results);

  void EnsureEnvironmentErrorHandler();
  void OnEnvironmentProviderError();

  const Member<XR> xr_;
  const SessionMode mode_;
  const bool environment_integration_;
  String blend_mode_string_;
  Member<XRRenderState> render_state_;
  Member<XRWorldTrackingState> world_tracking_state_;
  Member<XRWorldInformation> world_information_;
  HeapVector<Member<XRRenderStateInit>> pending_render_state_;

  WTF::Vector<XRViewData> views_;

  Member<XRInputSourceArray> input_sources_;
  Member<ResizeObserver> resize_observer_;
  Member<XRCanvasInputProvider> canvas_input_provider_;
  bool environment_error_handler_subscribed_ = false;
  HeapHashSet<Member<ScriptPromiseResolver>> hit_test_promises_;
  HeapVector<Member<XRReferenceSpace>> reference_spaces_;

  bool has_xr_focus_ = true;
  bool is_external_ = false;
  unsigned int display_info_id_ = 0;
  unsigned int stage_parameters_id_ = 0;
  device::mojom::blink::VRDisplayInfoPtr display_info_;

  mojo::Binding<device::mojom::blink::XRSessionClient> client_binding_;

  Member<XRFrameRequestCallbackCollection> callback_collection_;
  std::unique_ptr<TransformationMatrix> base_pose_matrix_;

  bool blurred_;
  bool ended_ = false;
  bool pending_frame_ = false;
  bool resolving_frame_ = false;
  bool update_views_next_frame_ = false;
  bool views_dirty_ = true;

  // Indicates that we've already logged a metric, so don't need to log it
  // again.
  mutable bool did_log_getInputSources_ = false;
  mutable bool did_log_getViewerPose_ = false;

  // Dimensions of the output canvas.
  int output_width_ = 1;
  int output_height_ = 1;

  // Indicates that this is a sensorless session which should only support the
  // identity reference space.
  bool sensorless_session_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
