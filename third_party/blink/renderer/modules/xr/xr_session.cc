// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_session.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_canvas_input_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_result.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"
#include "third_party/blink/renderer/modules/xr/xr_input_sources_change_event.h"
#include "third_party/blink/renderer/modules/xr/xr_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_ray.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state_init.h"
#include "third_party/blink/renderer/modules/xr/xr_session_event.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_world_information.h"
#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state.h"
#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state_init.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

namespace {

const char kSessionEnded[] = "XRSession has already ended.";

const char kReferenceSpaceNotSupported[] =
    "This device does not support the requested reference space type.";

const char kIncompatibleLayer[] =
    "XRLayer was created with a different session.";

const char kInlineVerticalFOVNotSupported[] =
    "This session does not support inlineVerticalFieldOfView";

const char kNoSpaceSpecified[] = "No XRSpace specified.";

const char kHitTestNotSupported[] = "Device does not support hit-test!";

const char kDeviceDisconnected[] = "The XR device has been disconnected.";

const double kDegToRad = M_PI / 180.0;

// Indices into the views array.
const unsigned int kMonoOrStereoLeftView = 0;
const unsigned int kStereoRightView = 1;

void UpdateViewFromEyeParameters(
    XRViewData& view,
    const device::mojom::blink::VREyeParametersPtr& eye,
    double depth_near,
    double depth_far) {
  const device::mojom::blink::VRFieldOfViewPtr& fov = eye->field_of_view;

  view.UpdateProjectionMatrixFromFoV(
      fov->up_degrees * kDegToRad, fov->down_degrees * kDegToRad,
      fov->left_degrees * kDegToRad, fov->right_degrees * kDegToRad, depth_near,
      depth_far);

  view.UpdateOffset(eye->offset.x(), eye->offset.y(), eye->offset.z());
}

}  // namespace

class XRSession::XRSessionResizeObserverDelegate final
    : public ResizeObserver::Delegate {
 public:
  explicit XRSessionResizeObserverDelegate(XRSession* session)
      : session_(session) {
    DCHECK(session);
  }
  ~XRSessionResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    session_->UpdateCanvasDimensions(entries[0]->target());
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(session_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<XRSession> session_;
};

XRSession::XRSession(
    XR* xr,
    device::mojom::blink::XRSessionClientRequest client_request,
    XRSession::SessionMode mode,
    EnvironmentBlendMode environment_blend_mode,
    bool uses_input_eventing,
    bool sensorless_session)
    : xr_(xr),
      mode_(mode),
      environment_integration_(mode == kModeImmersiveAR),
      world_tracking_state_(MakeGarbageCollected<XRWorldTrackingState>()),
      world_information_(MakeGarbageCollected<XRWorldInformation>(this)),
      input_sources_(MakeGarbageCollected<XRInputSourceArray>()),
      client_binding_(this, std::move(client_request)),
      input_binding_(this),
      callback_collection_(
          MakeGarbageCollected<XRFrameRequestCallbackCollection>(
              xr_->GetExecutionContext())),
      uses_input_eventing_(uses_input_eventing),
      sensorless_session_(sensorless_session) {
  render_state_ = MakeGarbageCollected<XRRenderState>(immersive());
  blurred_ = !HasAppropriateFocus();

  switch (environment_blend_mode) {
    case kBlendModeOpaque:
      blend_mode_string_ = "opaque";
      break;
    case kBlendModeAdditive:
      blend_mode_string_ = "additive";
      break;
    case kBlendModeAlphaBlend:
      blend_mode_string_ = "alpha-blend";
      break;
    default:
      NOTREACHED() << "Unknown environment blend mode: "
                   << environment_blend_mode;
  }
}

bool XRSession::immersive() const {
  return mode_ == kModeImmersiveVR || mode_ == kModeImmersiveAR;
}

ExecutionContext* XRSession::GetExecutionContext() const {
  return xr_->GetExecutionContext();
}

const AtomicString& XRSession::InterfaceName() const {
  return event_target_names::kXRSession;
}

device::mojom::blink::XRInputSourceButtonListenerAssociatedPtrInfo
XRSession::GetInputClickListener() {
  DCHECK(!input_binding_);
  device::mojom::blink::XRInputSourceButtonListenerAssociatedPtrInfo
      input_listener;
  input_binding_.Bind(MakeRequest(&input_listener));
  return input_listener;
}

void XRSession::updateRenderState(XRRenderStateInit* init,
                                  ExceptionState& exception_state) {
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return;
  }

  if (immersive() && init->hasInlineVerticalFieldOfView()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInlineVerticalFOVNotSupported);
    return;
  }

  if (init->hasBaseLayer() && init->baseLayer()) {
    // Validate that any baseLayer provided was created with this session.
    if (init->baseLayer()->session() != this) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        kIncompatibleLayer);
      return;
    }

    // If the baseLayer was previously null and there are outstanding rAF
    // callbacks, kick off a new frame request to flush them out.
    if (!render_state_->baseLayer() && !pending_frame_ &&
        !callback_collection_->IsEmpty()) {
      // Kick off a request for a new XR frame.
      xr_->frameProvider()->RequestFrame(this);
      pending_frame_ = true;
    }

    if (!immersive() && init->baseLayer()->output_canvas()) {
      // If the output canvas was previously null and there are outstanding rAF
      // callbacks, kick off a new frame request to flush them out.
      if (!render_state_->output_canvas() && !pending_frame_ &&
          !callback_collection_->IsEmpty()) {
        // Kick off a request for a new XR frame.
        xr_->frameProvider()->RequestFrame(this);
        pending_frame_ = true;
      }
    }
  }

  pending_render_state_.push_back(init);
}

void XRSession::updateWorldTrackingState(
    XRWorldTrackingStateInit* world_tracking_state_init,
    ExceptionState& exception_state) {
  DVLOG(3) << __func__;

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return;
  }

  world_tracking_state_ =
      MakeGarbageCollected<XRWorldTrackingState>(world_tracking_state_init);
}

void XRSession::UpdateEyeParameters(
    const device::mojom::blink::VREyeParametersPtr& left_eye,
    const device::mojom::blink::VREyeParametersPtr& right_eye) {
  auto display_info = display_info_.Clone();
  display_info->left_eye = left_eye.Clone();
  display_info->right_eye = right_eye.Clone();
  SetXRDisplayInfo(std::move(display_info));
}

void XRSession::UpdateStageParameters(
    const device::mojom::blink::VRStageParametersPtr& stage_parameters) {
  auto display_info = display_info_.Clone();
  display_info->stage_parameters = stage_parameters.Clone();
  SetXRDisplayInfo(std::move(display_info));
}

ScriptPromise XRSession::requestReferenceSpace(ScriptState* script_state,
                                               const String& type) {
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError, kSessionEnded));
  }

  XRReferenceSpace::Type requested_type =
      XRReferenceSpace::StringToReferenceSpaceType(type);

  UMA_HISTOGRAM_ENUMERATION("XR.WebXR.ReferenceSpace.Requested",
                            requested_type);

  if (sensorless_session_ &&
      requested_type != XRReferenceSpace::Type::kTypeViewer) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                           kReferenceSpaceNotSupported));
  }

  XRReferenceSpace* reference_space = nullptr;
  switch (requested_type) {
    case XRReferenceSpace::Type::kTypeViewer:
    case XRReferenceSpace::Type::kTypeLocal:
    case XRReferenceSpace::Type::kTypeLocalFloor:
      reference_space =
          MakeGarbageCollected<XRReferenceSpace>(this, requested_type);
      break;
    case XRReferenceSpace::Type::kTypeBoundedFloor: {
      bool supports_bounded = false;
      if (immersive() && display_info_->stage_parameters) {
        if (display_info_->stage_parameters->bounds) {
          supports_bounded = true;
        } else if (display_info_->stage_parameters->size_x > 0 &&
                   display_info_->stage_parameters->size_z > 0) {
          supports_bounded = true;
        }
      }

      if (supports_bounded) {
        reference_space = MakeGarbageCollected<XRBoundedReferenceSpace>(this);
      } else {
        return ScriptPromise::RejectWithDOMException(
            script_state, MakeGarbageCollected<DOMException>(
                              DOMExceptionCode::kNotSupportedError,
                              kReferenceSpaceNotSupported));
      }
      break;
    }
    case XRReferenceSpace::Type::kTypeUnbounded:
      if (immersive() && environment_integration_) {
        reference_space = MakeGarbageCollected<XRReferenceSpace>(
            this, XRReferenceSpace::Type::kTypeUnbounded);
      } else {
        return ScriptPromise::RejectWithDOMException(
            script_state, MakeGarbageCollected<DOMException>(
                              DOMExceptionCode::kNotSupportedError,
                              kReferenceSpaceNotSupported));
      }
      break;
  }

  DCHECK(reference_space);
  reference_spaces_.push_back(reference_space);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(reference_space);

  UMA_HISTOGRAM_ENUMERATION("XR.WebXR.ReferenceSpace.Succeeded",
                            requested_type);

  return promise;
}

int XRSession::requestAnimationFrame(V8XRFrameRequestCallback* callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  // Don't allow any new frame requests once the session is ended.
  if (ended_)
    return 0;

  int id = callback_collection_->RegisterCallback(callback);
  if (!pending_frame_) {
    // Kick off a request for a new XR frame.
    xr_->frameProvider()->RequestFrame(this);
    pending_frame_ = true;
  }
  return id;
}

void XRSession::cancelAnimationFrame(int id) {
  callback_collection_->CancelCallback(id);
}

XRInputSourceArray* XRSession::inputSources() const {
  Document* doc = To<Document>(GetExecutionContext());
  if (!did_log_getInputSources_ && doc) {
    ukm::builders::XR_WebXR(xr_->GetSourceId())
        .SetDidGetXRInputSources(1)
        .Record(doc->UkmRecorder());
    did_log_getInputSources_ = true;
  }

  return input_sources_;
}

ScriptPromise XRSession::requestHitTest(ScriptState* script_state,
                                        XRRay* ray,
                                        XRSpace* space) {
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError, kSessionEnded));
  }

  if (!space) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), kNoSpaceSpecified));
  }

  // Reject the promise if device doesn't support the hit-test API.
  // TODO(https://crbug.com/878936): Get the environment provider without going
  // up to xr_, since it doesn't know which runtime's environment provider
  // we want.
  if (!xr_->xrEnvironmentProviderPtr()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError,
                                           kHitTestNotSupported));
  }

  device::mojom::blink::XRRayPtr ray_mojo = device::mojom::blink::XRRay::New();

  ray_mojo->origin = WebFloatPoint3D(ray->origin()->x(), ray->origin()->y(),
                                     ray->origin()->z());

  ray_mojo->direction = {ray->direction()->x(), ray->direction()->y(),
                         ray->direction()->z()};

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  EnsureEnvironmentErrorHandler();
  xr_->xrEnvironmentProviderPtr()->RequestHitTest(
      std::move(ray_mojo),
      WTF::Bind(&XRSession::OnHitTestResults, WrapPersistent(this),
                WrapPersistent(resolver)));
  hit_test_promises_.insert(resolver);

  return promise;
}

void XRSession::OnHitTestResults(
    ScriptPromiseResolver* resolver,
    base::Optional<WTF::Vector<device::mojom::blink::XRHitResultPtr>> results) {
  DCHECK(hit_test_promises_.Contains(resolver));
  hit_test_promises_.erase(resolver);

  if (!results) {
    resolver->Reject();
    return;
  }

  HeapVector<Member<XRHitResult>> hit_results;
  for (const auto& mojom_result : results.value()) {
    XRHitResult* hit_result = MakeGarbageCollected<XRHitResult>(
        TransformationMatrix(mojom_result->hit_matrix.matrix()));
    hit_results.push_back(hit_result);
  }
  resolver->Resolve(hit_results);
}

void XRSession::EnsureEnvironmentErrorHandler() {
  if (!environment_error_handler_subscribed_ &&
      xr_->xrEnvironmentProviderPtr()) {
    environment_error_handler_subscribed_ = true;
    xr_->AddEnvironmentProviderErrorHandler(WTF::Bind(
        &XRSession::OnEnvironmentProviderError, WrapWeakPersistent(this)));
  }
}

void XRSession::OnEnvironmentProviderError() {
  HeapHashSet<Member<ScriptPromiseResolver>> hit_test_promises;
  hit_test_promises_.swap(hit_test_promises);
  for (ScriptPromiseResolver* resolver : hit_test_promises) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kDeviceDisconnected));
  }
}

ScriptPromise XRSession::end(ScriptState* script_state) {
  // Don't allow a session to end twice.
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError, kSessionEnded));
  }

  ForceEnd();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(bajones): If there's any work that needs to be done asynchronously on
  // session end it should be completed before this promise is resolved.

  resolver->Resolve();
  return promise;
}

void XRSession::ForceEnd() {
  // If we've already ended, then just abort.  Since this is called only by C++
  // code, and predominantly just to ensure that the session is shut down, this
  // is fine.
  if (ended_)
    return;

  // Detach this session from the XR system.
  ended_ = true;
  pending_frame_ = false;

  for (unsigned i = 0; i < input_sources_->length(); i++) {
    auto* input_source = (*input_sources_)[i];
    input_source->OnRemoved();
  }

  input_sources_ = nullptr;

  if (canvas_input_provider_) {
    canvas_input_provider_->Stop();
    canvas_input_provider_ = nullptr;
  }

  // If this session is the active immersive session, notify the frameProvider
  // that it's ended.
  if (xr_->frameProvider()->immersive_session() == this) {
    xr_->frameProvider()->OnImmersiveSessionEnded();
  }

  DispatchEvent(*XRSessionEvent::Create(event_type_names::kEnd, this));
}

double XRSession::NativeFramebufferScale() const {
  if (immersive()) {
    double scale = display_info_->webxr_default_framebuffer_scale;
    DCHECK(scale);

    // Return the inverse of the default scale, since that's what we'll need to
    // multiply the default size by to get back to the native size.
    return 1.0 / scale;
  }
  return 1.0;
}

DoubleSize XRSession::DefaultFramebufferSize() const {
  if (!immersive()) {
    return OutputCanvasSize();
  }

  double scale = display_info_->webxr_default_framebuffer_scale;
  double width = display_info_->left_eye->render_width;
  double height = display_info_->left_eye->render_height;

  if (display_info_->right_eye) {
    width += display_info_->right_eye->render_width;
    height = std::max(display_info_->left_eye->render_height,
                      display_info_->right_eye->render_height);
  }

  return DoubleSize(width * scale, height * scale);
}

DoubleSize XRSession::OutputCanvasSize() const {
  if (!render_state_->output_canvas()) {
    return DoubleSize();
  }

  return DoubleSize(output_width_, output_height_);
}

void XRSession::OnFocus() {
  if (!blurred_)
    return;

  blurred_ = false;
  DispatchEvent(*XRSessionEvent::Create(event_type_names::kFocus, this));
}

void XRSession::OnBlur() {
  if (blurred_)
    return;

  blurred_ = true;
  DispatchEvent(*XRSessionEvent::Create(event_type_names::kBlur, this));
}

// Immersive sessions may still not be blurred in headset even if the page isn't
// focused.  This prevents the in-headset experience from freezing on an
// external display headset when the user clicks on another tab.
bool XRSession::HasAppropriateFocus() {
  return immersive() ? has_xr_focus_ : has_xr_focus_ && xr_->IsFrameFocused();
}

void XRSession::OnFocusChanged() {
  if (HasAppropriateFocus()) {
    OnFocus();
  } else {
    OnBlur();
  }
}

void XRSession::DetachOutputCanvas(HTMLCanvasElement* canvas) {
  if (!canvas)
    return;

  // Remove anything in this session observing the given output canvas.
  if (resize_observer_) {
    resize_observer_->unobserve(canvas);
  }

  if (canvas_input_provider_ && canvas_input_provider_->canvas() == canvas) {
    canvas_input_provider_->Stop();
    canvas_input_provider_ = nullptr;
  }
}

void XRSession::ApplyPendingRenderState() {
  if (pending_render_state_.size() > 0) {
    XRLayer* prev_base_layer = render_state_->baseLayer();
    HTMLCanvasElement* prev_ouput_canvas = render_state_->output_canvas();
    update_views_next_frame_ = true;

    // Loop through each pending render state and apply it to the active one.
    for (auto& init : pending_render_state_) {
      render_state_->Update(init);
    }
    pending_render_state_.clear();

    // If this is an inline session and the base layer has changed, give it an
    // opportunity to update it's drawing buffer size.
    if (!immersive() && render_state_->baseLayer() &&
        render_state_->baseLayer() != prev_base_layer) {
      render_state_->baseLayer()->OnResize();
    }

    // If the output canvas changed, remove listeners from the old one and add
    // listeners to the new one as appropriate.
    if (prev_ouput_canvas != render_state_->output_canvas()) {
      // Remove anything observing the previous canvas.
      if (prev_ouput_canvas) {
        DetachOutputCanvas(prev_ouput_canvas);
      }

      // Monitor the new canvas for resize/input events.
      HTMLCanvasElement* canvas = render_state_->output_canvas();
      if (canvas) {
        if (!resize_observer_) {
          resize_observer_ = ResizeObserver::Create(
              canvas->GetDocument(),
              MakeGarbageCollected<XRSessionResizeObserverDelegate>(this));
        }
        resize_observer_->observe(canvas);

        // Begin processing input events on the output context's canvas.
        if (!immersive()) {
          canvas_input_provider_ =
              MakeGarbageCollected<XRCanvasInputProvider>(this, canvas);
        }

        // Get the new canvas dimensions
        UpdateCanvasDimensions(canvas);
      }
    }
  }
}

void XRSession::OnFrame(
    double timestamp,
    std::unique_ptr<TransformationMatrix> base_pose_matrix,
    const base::Optional<gpu::MailboxHolder>& output_mailbox_holder,
    const device::mojom::blink::XRPlaneDetectionDataPtr& detected_planes_data) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;
  // Don't process any outstanding frames once the session is ended.
  if (ended_)
    return;

  base_pose_matrix_ = std::move(base_pose_matrix);

  // If there are pending render state changes, apply them now.
  ApplyPendingRenderState();

  world_information_->ProcessPlaneInformation(detected_planes_data, timestamp);

  if (pending_frame_) {
    pending_frame_ = false;

    // Don't allow frames to be processed if there's no layers attached to the
    // session. That would allow tracking with no associated visuals.
    XRLayer* frame_base_layer = render_state_->baseLayer();
    if (!frame_base_layer)
      return;

    // Don't allow frames to be processed if an inline session doesn't have an
    // output canvas.
    if (!immersive() && !render_state_->output_canvas())
      return;

    XRFrame* presentation_frame = CreatePresentationFrame();
    presentation_frame->SetAnimationFrame(true);

    // Make sure that any frame-bounded changed to the views array take effect.
    if (update_views_next_frame_) {
      views_dirty_ = true;
      update_views_next_frame_ = false;
    }

    frame_base_layer->OnFrameStart(output_mailbox_holder);

    // Resolve the queued requestAnimationFrame callbacks. All XR rendering will
    // happen within these calls. resolving_frame_ will be true for the duration
    // of the callbacks.
    base::AutoReset<bool> resolving(&resolving_frame_, true);
    callback_collection_->ExecuteCallbacks(this, timestamp, presentation_frame);

    // The session might have ended in the middle of the frame. Only call
    // OnFrameEnd if it's still valid.
    if (!ended_)
      frame_base_layer->OnFrameEnd();

    // Ensure the XRFrame cannot be used outside the callbacks.
    presentation_frame->Deactivate();
  }
}

void XRSession::LogGetPose() const {
  Document* doc = To<Document>(GetExecutionContext());
  if (!did_log_getViewerPose_ && doc) {
    did_log_getViewerPose_ = true;

    ukm::builders::XR_WebXR(xr_->GetSourceId())
        .SetDidRequestPose(1)
        .Record(doc->UkmRecorder());
  }
}

XRFrame* XRSession::CreatePresentationFrame() {
  XRFrame* presentation_frame =
      MakeGarbageCollected<XRFrame>(this, world_information_);
  if (base_pose_matrix_) {
    presentation_frame->SetBasePoseMatrix(*base_pose_matrix_);
  }
  return presentation_frame;
}

// Called when the canvas element for this session's output context is resized.
void XRSession::UpdateCanvasDimensions(Element* element) {
  DCHECK(element);

  double devicePixelRatio = 1.0;
  LocalFrame* frame = xr_->GetFrame();
  if (frame) {
    devicePixelRatio = frame->DevicePixelRatio();
  }

  update_views_next_frame_ = true;
  output_width_ = element->OffsetWidth() * devicePixelRatio;
  output_height_ = element->OffsetHeight() * devicePixelRatio;
  int output_angle = 0;

  // TODO(crbug.com/836948): handle square canvases.
  // TODO(crbug.com/840346): we should not need to use ScreenOrientation here.
  ScreenOrientation* orientation = ScreenOrientation::Create(frame);

  if (orientation) {
    output_angle = orientation->angle();
    DVLOG(2) << __FUNCTION__ << ": got angle=" << output_angle;
  }

  if (render_state_->baseLayer()) {
    render_state_->baseLayer()->OnResize();
  }
}

void XRSession::OnButtonEvent(
    device::mojom::blink::XRInputSourceStatePtr input_state) {
  DCHECK(uses_input_eventing_);
  OnInputStateChangeInternal(last_frame_id_, base::make_span(&input_state, 1),
                             true /* from_eventing */);
}

void XRSession::OnInputStateChange(
    int16_t frame_id,
    base::span<const device::mojom::blink::XRInputSourceStatePtr>
        input_states) {
  OnInputStateChangeInternal(frame_id, input_states, false /* from_eventing */);
}

void XRSession::OnInputStateChangeInternal(
    int16_t frame_id,
    base::span<const device::mojom::blink::XRInputSourceStatePtr> input_states,
    bool from_eventing) {
  HeapVector<Member<XRInputSource>> added;
  HeapVector<Member<XRInputSource>> removed;
  last_frame_id_ = frame_id;

  // Build up our added array, and update the frame id of any active input
  // sources so we can flag the ones that are no longer active.
  for (const auto& input_state : input_states) {
    XRInputSource* stored_input_source =
        input_sources_->GetWithSourceId(input_state->source_id);
    XRInputSource* input_source = XRInputSource::CreateOrUpdateFrom(
        stored_input_source, this, input_state);

    // Using pointer equality to determine if the pointer needs to be set.
    if (stored_input_source != input_source) {
      input_sources_->SetWithSourceId(input_state->source_id, input_source);
      added.push_back(input_source);

      // If we previously had a stored_input_source, disconnect it's gamepad
      // and mark that it was removed.
      if (stored_input_source) {
        stored_input_source->SetGamepadConnected(false);
        removed.push_back(stored_input_source);
      }
    }

    input_source->setActiveFrameId(frame_id);
  }

  // Remove any input sources that are inactive, and disconnect their gamepad.
  // Note that this is done in two passes because HeapHashMap makes no
  // guarantees about iterators on removal.
  // We use a separate array of inactive sources here rather than just
  // processing removed, because if we replaced any input sources, they would
  // also be in removed, and we'd remove our newly added source.
  Vector<uint32_t> inactive_sources;
  for (unsigned i = 0; i < input_sources_->length(); i++) {
    auto* input_source = (*input_sources_)[i];
    if (input_source->activeFrameId() != frame_id) {
      inactive_sources.push_back(input_source->source_id());
      input_source->OnRemoved();
      removed.push_back(input_source);
    }
  }

  for (uint32_t source_id : inactive_sources) {
    input_sources_->RemoveWithSourceId(source_id);
  }

  // If there have been any changes, fire the input sources change event.
  if (!added.IsEmpty() || !removed.IsEmpty()) {
    DispatchEvent(*XRInputSourcesChangeEvent::Create(
        event_type_names::kInputsourceschange, this, added, removed));
  }

  // Now that we've fired the input sources change event (if needed), update
  // and fire events for any select state changes.
  for (const auto& input_state : input_states) {
    // If anything during the process of updating the select state caused us
    // to end our session, we should stop processing select state updates.
    if (ended_)
      break;

    // If this data is not from eventing and we support it, ignore it's click
    // states.
    // If this data is from eventing, but we don't support it, then ignore it
    if (from_eventing != uses_input_eventing_)
      continue;

    XRInputSource* input_source =
        input_sources_->GetWithSourceId(input_state->source_id);
    DCHECK(input_source);
    input_source->UpdateSelectState(input_state);
  }
}

void XRSession::AddTransientInputSource(XRInputSource* input_source) {
  if (ended_)
    return;

  // Ensure we're not overriding an input source that's already present.
  DCHECK(!input_sources_->GetWithSourceId(input_source->source_id()));
  input_sources_->SetWithSourceId(input_source->source_id(), input_source);

  DispatchEvent(*XRInputSourcesChangeEvent::Create(
      event_type_names::kInputsourceschange, this, {input_source}, {}));
}

void XRSession::RemoveTransientInputSource(XRInputSource* input_source) {
  if (ended_)
    return;

  input_sources_->RemoveWithSourceId(input_source->source_id());

  DispatchEvent(*XRInputSourcesChangeEvent::Create(
      event_type_names::kInputsourceschange, this, {}, {input_source}));
}

void XRSession::OnPoseReset() {
  for (const auto& reference_space : reference_spaces_) {
    reference_space->OnReset();
  }
}

void XRSession::OnChanged(device::mojom::blink::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  SetXRDisplayInfo(std::move(display_info));
}

void XRSession::OnExitPresent() {
  if (immersive()) {
    ForceEnd();
  }
}

void XRSession::SetXRDisplayInfo(
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  // We don't necessarily trust the backend to only send us display info changes
  // when something has actually changed, and a change here can trigger several
  // other interfaces to recompute data or fire events, so it's worthwhile to
  // validate that an actual change has occurred.
  if (display_info_) {
    if (display_info_->Equals(*display_info))
      return;

    if (display_info_->stage_parameters && display_info->stage_parameters &&
        !display_info_->stage_parameters->Equals(
            *(display_info->stage_parameters))) {
      // Stage parameters changed.
      stage_parameters_id_++;
    } else if (!!(display_info_->stage_parameters) !=
               !!(display_info->stage_parameters)) {
      // Either stage parameters just became available (sometimes happens if
      // detecting the bounds doesn't happen until a few seconds into the
      // session for platforms such as WMR), or the stage parameters just went
      // away (probably due to tracking loss).
      stage_parameters_id_++;
    }
  } else if (display_info && display_info->stage_parameters) {
    // Got stage parameters for the first time this session.
    stage_parameters_id_++;
  }

  display_info_id_++;
  display_info_ = std::move(display_info);
  is_external_ = display_info_->capabilities->has_external_display;
}

WTF::Vector<XRViewData>& XRSession::views() {
  // TODO(bajones): For now we assume that immersive sessions render a stereo
  // pair of views and non-immersive sessions render a single view. That doesn't
  // always hold true, however, so the view configuration should ultimately come
  // from the backing service.
  if (views_dirty_) {
    if (immersive()) {
      // If we don't already have the views allocated, do so now.
      if (views_.IsEmpty()) {
        views_.emplace_back(XRView::kEyeLeft);
        if (display_info_->right_eye) {
          views_.emplace_back(XRView::kEyeRight);
        }
      }
      // In immersive mode the projection and view matrices must be aligned with
      // the device's physical optics.
      UpdateViewFromEyeParameters(
          views_[kMonoOrStereoLeftView], display_info_->left_eye,
          render_state_->depthNear(), render_state_->depthFar());
      if (display_info_->right_eye) {
        UpdateViewFromEyeParameters(
            views_[kStereoRightView], display_info_->right_eye,
            render_state_->depthNear(), render_state_->depthFar());
      }
    } else {
      if (views_.IsEmpty()) {
        views_.emplace_back(XRView::kEyeNone);
        views_[kMonoOrStereoLeftView].UpdateOffset(0, 0, 0);
      }

      float aspect = 1.0f;
      if (output_width_ && output_height_) {
        aspect = static_cast<float>(output_width_) /
                 static_cast<float>(output_height_);
      }

      // In non-immersive mode, if there is no explicit projection matrix
      // provided, the projection matrix must be aligned with the
      // output canvas dimensions.
      bool is_null = true;
      double inline_vertical_fov =
          render_state_->inlineVerticalFieldOfView(is_null);

      // inlineVerticalFieldOfView should only be null in immersive mode.
      DCHECK(!is_null);
      views_[kMonoOrStereoLeftView].UpdateProjectionMatrixFromAspect(
          inline_vertical_fov, aspect, render_state_->depthNear(),
          render_state_->depthFar());
    }

    views_dirty_ = false;
  }

  return views_;
}

bool XRSession::HasPendingActivity() const {
  return !callback_collection_->IsEmpty() && !ended_;
}

void XRSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(render_state_);
  visitor->Trace(world_tracking_state_);
  visitor->Trace(world_information_);
  visitor->Trace(pending_render_state_);
  visitor->Trace(input_sources_);
  visitor->Trace(resize_observer_);
  visitor->Trace(canvas_input_provider_);
  visitor->Trace(callback_collection_);
  visitor->Trace(hit_test_promises_);
  visitor->Trace(reference_spaces_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
