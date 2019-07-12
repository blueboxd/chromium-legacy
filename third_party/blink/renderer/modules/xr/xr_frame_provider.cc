// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_plane_detection_state.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_viewport.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "ui/display/display.h"

namespace blink {

namespace {

class XRFrameProviderRequestCallback
    : public FrameRequestCallbackCollection::FrameCallback {
 public:
  explicit XRFrameProviderRequestCallback(XRFrameProvider* frame_provider)
      : frame_provider_(frame_provider) {}
  ~XRFrameProviderRequestCallback() override = default;
  void Invoke(double high_res_time_ms) override {
    frame_provider_->OnNonImmersiveVSync(high_res_time_ms);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(frame_provider_);

    FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
  }

  Member<XRFrameProvider> frame_provider_;
};

std::unique_ptr<TransformationMatrix> getPoseMatrix(
    const device::mojom::blink::VRPosePtr& pose) {
  if (!pose)
    return nullptr;

  return std::make_unique<TransformationMatrix>(
      mojo::TypeConverter<TransformationMatrix,
                          device::mojom::blink::VRPosePtr>::Convert(pose));
}

}  // namespace

XRFrameProvider::XRFrameProvider(XR* xr)
    : xr_(xr), last_has_focus_(xr->IsFrameFocused()) {
  frame_transport_ = MakeGarbageCollected<XRFrameTransport>();
}

void XRFrameProvider::BeginImmersiveSession(
    XRSession* session,
    device::mojom::blink::XRSessionPtr session_ptr) {
  // Make sure the session is indeed an immersive one.
  DCHECK(session && session->immersive());

  // Ensure we can only have one immersive session at a time.
  DCHECK(!immersive_session_);
  DCHECK(session_ptr->data_provider);
  DCHECK(session_ptr->submit_frame_sink);

  immersive_session_ = session;

  immersive_data_provider_.Bind(std::move(session_ptr->data_provider));
  immersive_data_provider_.set_connection_error_handler(WTF::Bind(
      &XRFrameProvider::OnProviderConnectionError, WrapWeakPersistent(this)));

  presentation_provider_.Bind(
      std::move(session_ptr->submit_frame_sink->provider));
  presentation_provider_.set_connection_error_handler(WTF::Bind(
      &XRFrameProvider::OnProviderConnectionError, WrapWeakPersistent(this)));

  frame_transport_->BindSubmitFrameClient(
      std::move(session_ptr->submit_frame_sink->client_request));
  frame_transport_->SetTransportOptions(
      std::move(session_ptr->submit_frame_sink->transport_options));
  frame_transport_->PresentChange();
}

void XRFrameProvider::OnFocusChanged() {
  bool focus = xr_->IsFrameFocused();

  // If we are gaining focus, schedule a frame for magic window.  This accounts
  // for skipping RAFs in ProcessScheduledFrame.  Only do this when there are
  // magic window sessions but no immersive session. Note that immersive
  // sessions don't stop scheduling RAFs when focus is lost, so there is no need
  // to schedule immersive frames when focus is acquired.
  if (focus && !last_has_focus_ && requesting_sessions_.size() > 0 &&
      !immersive_session_) {
    ScheduleNonImmersiveFrame(nullptr);
  }
  last_has_focus_ = focus;
}

// Ends the immersive session when the presentation or immersive data provider
// got disconnected.
void XRFrameProvider::OnProviderConnectionError() {
  presentation_provider_.reset();
  immersive_data_provider_.reset();
  if (vsync_connection_failed_)
    return;
  immersive_session_->ForceEnd();
  vsync_connection_failed_ = true;
}

// Called by the immersive session when it is ended.
void XRFrameProvider::OnImmersiveSessionEnded() {
  if (!immersive_session_)
    return;

  xr_->ExitPresent();

  immersive_session_ = nullptr;
  pending_immersive_vsync_ = false;
  frame_id_ = -1;
  presentation_provider_.reset();
  immersive_data_provider_.reset();

  frame_transport_ = MakeGarbageCollected<XRFrameTransport>();

  // When we no longer have an active immersive session schedule all the
  // outstanding frames that were requested while the immersive session was
  // active.
  if (requesting_sessions_.size() > 0)
    ScheduleNonImmersiveFrame(nullptr);
}

// Schedule a session to be notified when the next XR frame is available.
void XRFrameProvider::RequestFrame(XRSession* session) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(session);

  auto options = device::mojom::blink::XRFrameDataRequestOptions::New(
      session->worldTrackingState()->planeDetectionState()->enabled());

  // Immersive frame logic.
  if (session->immersive()) {
    ScheduleImmersiveFrame(std::move(options));
    return;
  }

  // Non-immersive frame logic.

  requesting_sessions_.push_back(session);

  // If there's an active immersive session save the request but suppress
  // processing it until the immersive session is no longer active.
  if (immersive_session_)
    return;

  ScheduleNonImmersiveFrame(std::move(options));
}

void XRFrameProvider::ScheduleImmersiveFrame(
    device::mojom::blink::XRFrameDataRequestOptionsPtr options) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  if (pending_immersive_vsync_)
    return;

  pending_immersive_vsync_ = true;

  immersive_data_provider_->GetFrameData(
      std::move(options), WTF::Bind(&XRFrameProvider::OnImmersiveFrameData,
                                    WrapWeakPersistent(this)));
}

void XRFrameProvider::ScheduleNonImmersiveFrame(
    device::mojom::blink::XRFrameDataRequestOptionsPtr options) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(!immersive_session_)
      << "Scheduling should be done via the exclusive session if present.";

  if (pending_non_immersive_vsync_)
    return;

  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;

  // TODO(https://crbug.com/856224) Review the lifetime of this object and
  // ensure that doc can never be null and remove this check.
  Document* doc = frame->GetDocument();
  if (!doc)
    return;

  // This is cleared by either OnNonImmersiveFrameData (GetFrameData callback,
  // used if we have a magic window provider), or by OnNonImmersiveVSync
  // (XRFrameProviderRequestCallback's invoke, used if there's no magic window
  // provider).
  pending_non_immersive_vsync_ = true;

  // If we have a Magic Window provider, request frame data and flag that
  // we're waiting for it.  If not, clear any pose data, so that
  // ProcessScheduledFrame handles it appropriately.
  if (xr_->CanRequestNonImmersiveFrameData()) {
    xr_->GetNonImmersiveFrameData(
        std::move(options), WTF::Bind(&XRFrameProvider::OnNonImmersiveFrameData,
                                      WrapWeakPersistent(this)));
  } else {
    frame_pose_ = nullptr;
  }

  doc->RequestAnimationFrame(
      MakeGarbageCollected<XRFrameProviderRequestCallback>(this));
}

void XRFrameProvider::OnImmersiveFrameData(
    device::mojom::blink::XRFrameDataPtr data) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;
  vsync_connection_failed_ = false;
  if (!data) {
    return;
  }

  // We may have lost the immersive session since the last VSync request.
  if (!immersive_session_) {
    return;
  }

  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;
  Document* doc = frame->GetDocument();
  if (!doc)
    return;

  base::TimeTicks monotonic_time_now = base::TimeTicks() + data->time_delta;
  double high_res_now_ms =
      doc->Loader()
          ->GetTiming()
          .MonotonicTimeToZeroBasedDocumentTime(monotonic_time_now)
          .InMillisecondsF();

  frame_pose_ = std::move(data->pose);
  frame_id_ = data->frame_id;
  buffer_mailbox_holder_ = data->buffer_holder;

  pending_immersive_vsync_ = false;

  // Post a task to handle scheduled animations after the current
  // execution context finishes, so that we yield to non-mojo tasks in
  // between frames. Executing mojo tasks back to back within the same
  // execution context caused extreme input delay due to processing
  // multiple frames without yielding, see crbug.com/701444.
  //
  // Used kInternalMedia since 1) this is not spec-ed and 2) this is media
  // related then tasks should not be throttled or frozen in background tabs.
  frame->GetTaskRunner(blink::TaskType::kInternalMedia)
      ->PostTask(FROM_HERE, WTF::Bind(&XRFrameProvider::ProcessScheduledFrame,
                                      WrapWeakPersistent(this), std::move(data),
                                      high_res_now_ms));
}

void XRFrameProvider::OnNonImmersiveVSync(double high_res_now_ms) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;

  pending_non_immersive_vsync_ = false;

  // Suppress non-immersive vsyncs when there's an immersive session active.
  if (immersive_session_)
    return;

  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;

  frame->GetTaskRunner(blink::TaskType::kInternalMedia)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&XRFrameProvider::ProcessScheduledFrame,
                           WrapWeakPersistent(this), nullptr, high_res_now_ms));
}

void XRFrameProvider::OnNonImmersiveFrameData(
    device::mojom::blink::XRFrameDataPtr frame_data) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;

  // TODO(https://crbug.com/837834): add unit tests for this code path.

  pending_non_immersive_vsync_ = false;
  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;
  Document* doc = frame->GetDocument();
  if (!doc)
    return;

  if (!frame_data) {
    // Unexpectedly didn't get frame data, and we don't have a timestamp.
    // Try to request a regular animation frame to avoid getting stuck.
    DVLOG(1) << __FUNCTION__ << ": NO FRAME DATA!";
    frame_pose_ = nullptr;
    doc->RequestAnimationFrame(
        MakeGarbageCollected<XRFrameProviderRequestCallback>(this));
    return;
  }

  frame_pose_ = std::move(frame_data->pose);
}

void XRFrameProvider::ProcessScheduledFrame(
    device::mojom::blink::XRFrameDataPtr frame_data,
    double high_res_now_ms) {
  DVLOG(2) << __FUNCTION__;

  TRACE_EVENT2("gpu", "XRFrameProvider::ProcessScheduledFrame", "frame",
               frame_id_, "timestamp", high_res_now_ms);

  if (!xr_->IsFrameFocused() && !immersive_session_) {
    return;  // Not currently focused, so we won't expose poses (except to
             // immersive sessions).
  }

  if (immersive_session_) {
    if (frame_pose_) {
      base::span<const device::mojom::blink::XRInputSourceStatePtr>
          input_states;
      if (frame_pose_->input_state.has_value())
        input_states = frame_pose_->input_state.value();

      immersive_session_->OnInputStateChange(frame_id_, input_states);
    }

    // Check if immersive session is still set as OnInputStateChange may have
    // allowed a ForceEndSession to be triggered.
    if (!immersive_session_)
      return;

    if (frame_pose_ && frame_pose_->pose_reset) {
      immersive_session_->OnPoseReset();
    }

    // Check if immersive session is still set as OnPoseReset may have allowed a
    // ForceEndSession to be triggered.
    if (!immersive_session_)
      return;

    // If there's an immersive session active only process its frame.
    std::unique_ptr<TransformationMatrix> pose_matrix =
        getPoseMatrix(frame_pose_);
#if DCHECK_IS_ON()
    // Sanity check: if drawing into a shared buffer, the optional mailbox
    // holder must be present. Exception is the first immersive frame after a
    // transition where the frame ID wasn't set yet. In that case, drawing can
    // proceed, but the result will be discarded in SubmitWebGLLayer().
    if (frame_transport_->DrawingIntoSharedBuffer() && frame_id_ >= 0) {
      DCHECK(buffer_mailbox_holder_);
    }
#endif
    if (frame_data && (frame_data->left_eye || frame_data->right_eye)) {
      immersive_session_->UpdateEyeParameters(frame_data->left_eye,
                                              frame_data->right_eye);
    }

    if (frame_data && frame_data->stage_parameters_updated) {
      immersive_session_->UpdateStageParameters(frame_data->stage_parameters);
    }
    if (frame_data) {
      immersive_session_->OnFrame(high_res_now_ms, std::move(pose_matrix),
                                  buffer_mailbox_holder_,
                                  frame_data->detected_planes_data);
    } else {
      immersive_session_->OnFrame(high_res_now_ms, std::move(pose_matrix),
                                  buffer_mailbox_holder_, nullptr);
    }
  } else {
    // In the process of fulfilling the frame requests for each session they are
    // extremely likely to request another frame. Work off of a separate list
    // from the requests to prevent infinite loops.
    DCHECK(processing_sessions_.IsEmpty());
    swap(requesting_sessions_, processing_sessions_);

    // Inform sessions with a pending request of the new frame
    for (unsigned i = 0; i < processing_sessions_.size(); ++i) {
      XRSession* session = processing_sessions_.at(i).Get();

      // If the session was terminated between requesting and now, we shouldn't
      // process anything further.
      if (session->ended())
        continue;

      if (frame_pose_) {
        base::span<const device::mojom::blink::XRInputSourceStatePtr>
            input_states;
        if (frame_pose_->input_state.has_value())
          input_states = frame_pose_->input_state.value();

        session->OnInputStateChange(frame_id_, input_states);
      }

      // If the input state change caused this session to end, we should stop
      // processing.
      if (session->ended())
        continue;

      if (frame_pose_ && frame_pose_->pose_reset) {
        session->OnPoseReset();
      }

      // If the pose reset caused us to end, we should stop processing.
      if (session->ended())
        continue;

      std::unique_ptr<TransformationMatrix> pose_matrix =
          getPoseMatrix(frame_pose_);
      if (frame_data) {
        session->OnFrame(high_res_now_ms, std::move(pose_matrix), base::nullopt,
                         frame_data->detected_planes_data);
      } else {
        session->OnFrame(high_res_now_ms, std::move(pose_matrix), base::nullopt,
                         nullptr);
      }
    }

    processing_sessions_.clear();
  }
}

void XRFrameProvider::SubmitWebGLLayer(XRWebGLLayer* layer, bool was_changed) {
  DCHECK(layer);
  DCHECK(immersive_session_);
  DCHECK(layer->session() == immersive_session_);
  if (!presentation_provider_.is_bound())
    return;

  TRACE_EVENT1("gpu", "XRFrameProvider::SubmitWebGLLayer", "frame", frame_id_);

  WebGLRenderingContextBase* webgl_context = layer->context();

  if (frame_id_ < 0) {
    // There is no valid frame_id_, and the browser side is not currently
    // expecting a frame to be submitted. That can happen for the first
    // immersive frame if the animation loop submits without a preceding
    // immersive GetFrameData response, in that case frame_id_ is -1 (see
    // https://crbug.com/855722).
    return;
  }

  if (!was_changed) {
    // Just tell the device side that there was no submitted frame instead of
    // executing the implicit end-of-frame submit.
    frame_transport_->FrameSubmitMissing(presentation_provider_.get(),
                                         webgl_context->ContextGL(), frame_id_);
    return;
  }

  frame_transport_->FramePreImage(webgl_context->ContextGL());

  std::unique_ptr<viz::SingleReleaseCallback> image_release_callback;

  if (frame_transport_->DrawingIntoSharedBuffer()) {
    // Image is written to shared buffer already. Just submit with a
    // placeholder.
    scoped_refptr<Image> image_ref = nullptr;
    bool needs_copy = false;
    DVLOG(3) << __FUNCTION__ << ": FrameSubmit for SharedBuffer mode";
    frame_transport_->FrameSubmit(
        presentation_provider_.get(), webgl_context->ContextGL(), webgl_context,
        std::move(image_ref), std::move(image_release_callback), frame_id_,
        needs_copy);
    return;
  }

  scoped_refptr<StaticBitmapImage> image_ref =
      layer->TransferToStaticBitmapImage(&image_release_callback);

  if (!image_ref)
    return;

  // Hardware-accelerated rendering should always be texture backed. Ensure this
  // is the case, don't attempt to render if using an unexpected drawing path.
  if (!image_ref->IsTextureBacked()) {
    NOTREACHED() << "WebXR requires hardware-accelerated rendering to texture";
    return;
  }

  // TODO(bajones): Remove this when the Windows path has been updated to no
  // longer require a texture copy.
  bool needs_copy = immersive_session_->External();

  frame_transport_->FrameSubmit(
      presentation_provider_.get(), webgl_context->ContextGL(), webgl_context,
      std::move(image_ref), std::move(image_release_callback), frame_id_,
      needs_copy);

  // Reset our frame id, since anything we'd want to do (resizing/etc) can
  // no-longer happen to this frame.
  frame_id_ = -1;
}

// TODO(bajones): This only works because we're restricted to a single layer at
// the moment. Will need an overhaul when we get more robust layering support.
void XRFrameProvider::UpdateWebGLLayerViewports(XRWebGLLayer* layer) {
  DCHECK(layer->session() == immersive_session_);
  DCHECK(presentation_provider_);

  XRViewport* left = layer->GetViewportForEye(XRView::kEyeLeft);
  XRViewport* right = layer->GetViewportForEye(XRView::kEyeRight);
  float width = layer->framebufferWidth();
  float height = layer->framebufferHeight();

  // We may only have one eye view, i.e. in smartphone immersive AR mode.
  // Use all-zero bounds for unused views.
  WebFloatRect left_coords =
      left ? WebFloatRect(
                 static_cast<float>(left->x()) / width,
                 static_cast<float>(height - (left->y() + left->height())) /
                     height,
                 static_cast<float>(left->width()) / width,
                 static_cast<float>(left->height()) / height)
           : WebFloatRect();
  WebFloatRect right_coords =
      right ? WebFloatRect(
                  static_cast<float>(right->x()) / width,
                  static_cast<float>(height - (right->y() + right->height())) /
                      height,
                  static_cast<float>(right->width()) / width,
                  static_cast<float>(right->height()) / height)
            : WebFloatRect();

  presentation_provider_->UpdateLayerBounds(
      frame_id_, left_coords, right_coords, WebSize(width, height));
}

void XRFrameProvider::Dispose() {
  presentation_provider_.reset();
  immersive_data_provider_.reset();
  // TODO(bajones): Do something for outstanding frame requests?
}

void XRFrameProvider::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(frame_transport_);
  visitor->Trace(immersive_session_);
  visitor->Trace(requesting_sessions_);
  visitor->Trace(processing_sessions_);
}

}  // namespace blink
