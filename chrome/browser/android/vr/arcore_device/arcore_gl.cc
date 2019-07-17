// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <utility>
#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/traced_value.h"
#include "chrome/browser/android/vr/arcore_device/ar_image_transport.h"
#include "chrome/browser/android/vr/arcore_device/arcore_impl.h"
#include "chrome/browser/android/vr/arcore_device/arcore_session_utils.h"
#include "chrome/browser/android/vr/web_xr_presentation_state.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace {
// Input display coordinates (range 0..1) used with ArCore's
// transformDisplayUvCoords to calculate the output matrix.
constexpr std::array<float, 6> kDisplayCoordinatesForTransform = {
    0.f, 0.f, 1.f, 0.f, 0.f, 1.f};

gfx::Transform ConvertUvsToTransformMatrix(const std::vector<float>& uvs) {
  // We're creating a matrix that transforms viewport UV coordinates (for a
  // screen-filling quad, origin at bottom left, u=1 at right, v=1 at top) to
  // camera texture UV coordinates. This matrix is used to compute texture
  // coordinates for copying an appropriately cropped and rotated subsection of
  // the camera image. The SampleData is a bit unfortunate. ArCore doesn't
  // provide a way to get a matrix directly. There's a function to transform UV
  // vectors individually, which obviously can't be used from a shader, so we
  // run that on selected vectors and recreate the matrix from the result.

  // Assumes that |uvs| is the result of transforming the display coordinates
  // from kDisplayCoordinatesForTransform. This combines the solved matrix with
  // a Y flip because ArCore's "normalized screen space" coordinates have the
  // origin at the top left to match 2D Android APIs, so it needs a Y flip to
  // get an origin at bottom left as used for textures.
  DCHECK_EQ(uvs.size(), 6U);
  float u00 = uvs[0];
  float v00 = uvs[1];
  float u10 = uvs[2];
  float v10 = uvs[3];
  float u01 = uvs[4];
  float v01 = uvs[5];

  // Transform initializes to the identity matrix and then is modified by uvs.
  gfx::Transform result;
  result.matrix().set(0, 0, u10 - u00);
  result.matrix().set(0, 1, -(u01 - u00));
  result.matrix().set(0, 3, u01);
  result.matrix().set(1, 0, v10 - v00);
  result.matrix().set(1, 1, -(v01 - v00));
  result.matrix().set(1, 3, v01);
  return result;
}

gfx::Transform WebXRImageTransformMatrix() {
  gfx::Transform result;
  return result;
}

const gfx::Size kDefaultFrameSize = {1, 1};
const display::Display::Rotation kDefaultRotation = display::Display::ROTATE_0;

}  // namespace

namespace device {

struct ArCoreHitTestRequest {
  ArCoreHitTestRequest() = default;
  ~ArCoreHitTestRequest() = default;
  mojom::XRRayPtr ray;
  mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArCoreHitTestRequest);
};

ArCoreGl::ArCoreGl(std::unique_ptr<ArImageTransport> ar_image_transport)
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      ar_image_transport_(std::move(ar_image_transport)),
      webxr_(std::make_unique<vr::WebXrPresentationState>()),
      frame_data_binding_(this),
      session_controller_binding_(this),
      environment_binding_(this),
      presentation_binding_(this),
      weak_ptr_factory_(this) {
  DVLOG(1) << __func__;
  webxr_transform_ = WebXRImageTransformMatrix();
}

ArCoreGl::~ArCoreGl() {
  DVLOG(1) << __func__;
  DCHECK(IsOnGlThread());
  ar_image_transport_.reset();
  CloseBindingsIfOpen();
}

void ArCoreGl::Initialize(vr::ArCoreSessionUtils* session_utils,
                          ArCoreFactory* arcore_factory,
                          gfx::AcceleratedWidget drawing_widget,
                          const gfx::Size& frame_size,
                          display::Display::Rotation display_rotation,
                          base::OnceCallback<void(bool)> callback) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);

  transfer_size_ = frame_size;
  display_rotation_ = display_rotation;
  should_update_display_geometry_ = true;

  if (!InitializeGl(drawing_widget)) {
    std::move(callback).Run(false);
    return;
  }

  // Get the activity context.
  base::android::ScopedJavaLocalRef<jobject> application_context =
      session_utils->GetApplicationContext();
  if (!application_context.obj()) {
    DLOG(ERROR) << "Unable to retrieve the Java context/activity!";
    std::move(callback).Run(false);
    return;
  }

  arcore_ = arcore_factory->Create();
  if (!arcore_->Initialize(application_context)) {
    DLOG(ERROR) << "ARCore failed to initialize";
    std::move(callback).Run(false);
    return;
  }

  // Set the texture on ArCore to render the camera.
  arcore_->SetCameraTexture(ar_image_transport_->GetCameraTextureId());
  // Set the Geometry to ensure consistent behaviour.
  arcore_->SetDisplayGeometry(kDefaultFrameSize, kDefaultRotation);

  is_initialized_ = true;

  std::move(callback).Run(true);
}

void ArCoreGl::CreateSession(mojom::VRDisplayInfoPtr display_info,
                             ArCoreGlCreateSessionCallback create_callback,
                             base::OnceClosure shutdown_callback) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  session_shutdown_callback_ = std::move(shutdown_callback);

  CloseBindingsIfOpen();

  mojom::XRFrameDataProviderPtrInfo frame_data_provider_info;
  frame_data_binding_.Bind(mojo::MakeRequest(&frame_data_provider_info));
  frame_data_binding_.set_connection_error_handler(base::BindOnce(
      &ArCoreGl::OnBindingDisconnect, weak_ptr_factory_.GetWeakPtr()));

  mojom::XRSessionControllerPtrInfo controller_info;
  session_controller_binding_.Bind(mojo::MakeRequest(&controller_info));
  session_controller_binding_.set_connection_error_handler(base::BindOnce(
      &ArCoreGl::OnBindingDisconnect, weak_ptr_factory_.GetWeakPtr()));

  device::mojom::XRPresentationProviderPtr presentation_provider;
  presentation_binding_.Bind(mojo::MakeRequest(&presentation_provider));

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  transport_options->wait_for_gpu_fence = true;

  // Currently, AR mode only supports Android O+ due to requiring
  // AHardwareBuffer-backed GpuMemoryBuffer shared images. This could be
  // extended back to Android N by using the SUBMIT_AS_MAILBOX_HOLDER method
  // that uses Surface/SurfaceTexture.
  transport_options->transport_method =
      device::mojom::XRPresentationTransportMethod::DRAW_INTO_TEXTURE_MAILBOX;

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->client_request = mojo::MakeRequest(&submit_client_);
  submit_frame_sink->provider = presentation_provider.PassInterface();
  submit_frame_sink->transport_options = std::move(transport_options);

  display_info_ = std::move(display_info);

  std::move(create_callback)
      .Run(std::move(frame_data_provider_info), display_info_->Clone(),
           std::move(controller_info), std::move(submit_frame_sink));
}

bool ArCoreGl::InitializeGl(gfx::AcceleratedWidget drawing_widget) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);

  if (gl::GetGLImplementation() == gl::kGLImplementationNone &&
      !gl::init::InitializeGLOneOff()) {
    DLOG(ERROR) << "gl::init::InitializeGLOneOff failed";
    return false;
  }

  scoped_refptr<gl::GLSurface> surface =
      gl::init::CreateViewGLSurface(drawing_widget);
  DVLOG(3) << "surface=" << surface.get();
  if (!surface.get()) {
    DLOG(ERROR) << "gl::init::CreateViewGLSurface failed";
    return false;
  }

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  if (!context.get()) {
    DLOG(ERROR) << "gl::init::CreateGLContext failed";
    return false;
  }
  if (!context->MakeCurrent(surface.get())) {
    DLOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }

  DVLOG(3) << "ar_image_transport_->Initialize()...";
  if (!ar_image_transport_->Initialize(webxr_.get())) {
    DLOG(ERROR) << "ARImageTransport failed to initialize";
    return false;
  }

  // Assign the surface and context members now that initialization has
  // succeeded.
  surface_ = std::move(surface);
  context_ = std::move(context);

  DVLOG(3) << "done";
  return true;
}

void ArCoreGl::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);

  if (webxr_->HaveAnimatingFrame()) {
    DVLOG(3) << __func__ << ": deferring, HaveAnimatingFrame";
    pending_getframedata_ =
        base::BindOnce(&ArCoreGl::GetFrameData, GetWeakPtr(),
                       std::move(options), std::move(callback));
    return;
  }

  DVLOG(3) << __func__ << ": should_update_display_geometry_="
           << should_update_display_geometry_
           << ", transfer_size_=" << transfer_size_.ToString()
           << ", display_rotation_=" << display_rotation_;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  if (restrict_frame_data_) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (is_paused_) {
    DVLOG(2) << __func__ << ": paused but frame data not restricted. Resuming.";
    Resume();
  }

  // Check if the frame_size and display_rotation updated last frame. If yes,
  // apply the update for this frame. In the current implementation, this should
  // only happen once per session since we don't support mid-session rotation or
  // resize.
  if (should_recalculate_uvs_) {
    // Get the UV transform matrix from ArCore's UV transform.
    std::vector<float> uvs_transformed =
        arcore_->TransformDisplayUvCoords(kDisplayCoordinatesForTransform);
    uv_transform_ = ConvertUvsToTransformMatrix(uvs_transformed);

    // We need near/far distances to make a projection matrix. The actual
    // values don't matter, the Renderer will recalculate dependent values
    // based on the application's near/far settngs.
    constexpr float depth_near = 0.1f;
    constexpr float depth_far = 1000.f;
    projection_ = arcore_->GetProjectionMatrix(depth_near, depth_far);
    auto m = projection_.matrix();
    float left = depth_near * (m.get(2, 0) - 1.f) / m.get(0, 0);
    float right = depth_near * (m.get(2, 0) + 1.f) / m.get(0, 0);
    float bottom = depth_near * (m.get(2, 1) - 1.f) / m.get(1, 1);
    float top = depth_near * (m.get(2, 1) + 1.f) / m.get(1, 1);

    // Also calculate the inverse projection which is needed for converting
    // screen touches to world rays.
    bool has_inverse = projection_.GetInverse(&inverse_projection_);
    DCHECK(has_inverse);

    // VRFieldOfView wants positive angles.
    mojom::VRFieldOfViewPtr field_of_view = mojom::VRFieldOfView::New();
    field_of_view->left_degrees = gfx::RadToDeg(atanf(-left / depth_near));
    field_of_view->right_degrees = gfx::RadToDeg(atanf(right / depth_near));
    field_of_view->down_degrees = gfx::RadToDeg(atanf(-bottom / depth_near));
    field_of_view->up_degrees = gfx::RadToDeg(atanf(top / depth_near));
    DVLOG(3) << " fov degrees up=" << field_of_view->up_degrees
             << " down=" << field_of_view->down_degrees
             << " left=" << field_of_view->left_degrees
             << " right=" << field_of_view->right_degrees;

    display_info_->left_eye->field_of_view = std::move(field_of_view);
    display_info_changed_ = true;

    should_recalculate_uvs_ = false;
  }

  // Now check if the frame_size or display_rotation needs to be updated
  // for the next frame. This must happen after the should_recalculate_uvs_
  // check above to ensure it executes with the needed one-frame delay.
  // The delay is needed due to the fact that ArCoreImpl already got a frame
  // and we don't want to calculate uvs for stale frame with new geometry.
  if (should_update_display_geometry_) {
    // Set display geometry before calling Update. It's a pending request that
    // applies to the next frame.
    arcore_->SetDisplayGeometry(transfer_size_, display_rotation_);

    // Tell the uvs to recalculate on the next animation frame, by which time
    // SetDisplayGeometry will have set the new values in arcore_.
    should_recalculate_uvs_ = true;
    should_update_display_geometry_ = false;
  }

  TRACE_EVENT_BEGIN0("gpu", "ArCore Update");
  bool camera_updated = false;
  mojom::VRPosePtr pose = arcore_->Update(&camera_updated);
  TRACE_EVENT_END0("gpu", "ArCore Update");
  if (!camera_updated) {
    DVLOG(1) << "arcore_->Update() failed";
    std::move(callback).Run(nullptr);
    have_camera_image_ = false;
    return;
  }

  // First frame will be requested without a prior call to SetDisplayGeometry -
  // handle this case.
  if (transfer_size_.IsEmpty()) {
    DLOG(ERROR) << "No valid AR frame size provided!";
    std::move(callback).Run(nullptr);
    have_camera_image_ = false;
    return;
  }

  have_camera_image_ = true;
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();

  frame_data->frame_id = webxr_->StartFrameAnimating();
  DVLOG(2) << __func__ << " frame=" << frame_data->frame_id;

  if (display_info_changed_) {
    frame_data->left_eye = display_info_->left_eye.Clone();
    display_info_changed_ = false;
  }
  // Set up a shared buffer for the renderer to draw into, it'll be sent
  // alongside the frame pose.
  gpu::MailboxHolder buffer_holder =
      ar_image_transport_->TransferFrame(transfer_size_, uv_transform_);

  if (pose) {
    mojom::XRInputSourceStatePtr input_state = GetInputSourceState();
    if (input_state) {
      input_states_.push_back(std::move(input_state));
      pose->input_state = std::move(input_states_);
    }
  }

  // Create the frame data to return to the renderer.
  frame_data->pose = std::move(pose);
  frame_data->buffer_holder = buffer_holder;
  frame_data->time_delta = base::TimeTicks::Now() - base::TimeTicks();

  if (options && options->include_plane_data) {
    frame_data->detected_planes_data = arcore_->GetDetectedPlanesData();
  }

  fps_meter_.AddFrame(base::TimeTicks::Now());
  TRACE_COUNTER1("gpu", "WebXR FPS", fps_meter_.GetFPS());

  // Post a task to finish processing the frame so any calls to
  // RequestHitTest() that were made during this function, which can block
  // on the arcore_->Update() call above, can be processed in this frame.
  gl_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ArCoreGl::ProcessFrame, weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&frame_data), base::Passed(&callback)));
}

bool ArCoreGl::IsSubmitFrameExpected(int16_t frame_index) {
  // submit_client_ could be null when we exit presentation, if there were
  // pending SubmitFrame messages queued.  XRSessionClient::OnExitPresent
  // will clean up state in blink, so it doesn't wait for
  // OnSubmitFrameTransferred or OnSubmitFrameRendered. Similarly,
  // the animating frame state is cleared when exiting presentation,
  // and we should ignore a leftover queued SubmitFrame.
  if (!submit_client_.get() || !webxr_->HaveAnimatingFrame())
    return false;

  vr::WebXrFrame* animating_frame = webxr_->GetAnimatingFrame();

  if (animating_frame->index != frame_index) {
    DVLOG(1) << __func__ << ": wrong frame index, got " << frame_index
             << ", expected " << animating_frame->index;
    mojo::ReportBadMessage("SubmitFrame called with wrong frame index");
    CloseBindingsIfOpen();
    return false;
  }

  // Frame looks valid.
  return true;
}

void ArCoreGl::SubmitFrameMissing(int16_t frame_index,
                                  const gpu::SyncToken& sync_token) {
  DVLOG(2) << __func__;

  if (!IsSubmitFrameExpected(frame_index))
    return;

  webxr_->RecycleUnusedAnimatingFrame();
  ar_image_transport_->WaitSyncToken(sync_token);

  // Draw the current camera texture to the output default framebuffer now.
  if (have_camera_image_) {
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
    ar_image_transport_->CopyCameraImageToFramebuffer(transfer_size_,
                                                      uv_transform_);
    have_camera_image_ = false;
  }

  // We're done with the camera image for this frame, start the next ARCore
  // update if we had deferred it. This will get the next frame's camera image
  // and pose in parallel while we're waiting for this frame's rendered image.
  if (pending_getframedata_) {
    std::move(pending_getframedata_).Run();
  }

  surface_->SwapBuffers(base::DoNothing());
  DVLOG(3) << __func__ << ": frame=" << frame_index << " SwapBuffers";
}

void ArCoreGl::SubmitFrame(int16_t frame_index,
                           const gpu::MailboxHolder& mailbox,
                           base::TimeDelta time_waited) {
  NOTIMPLEMENTED();
}

void ArCoreGl::SubmitFrameWithTextureHandle(int16_t frame_index,
                                            mojo::ScopedHandle texture_handle) {
  NOTIMPLEMENTED();
}

void ArCoreGl::SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                           const gpu::SyncToken& sync_token,
                                           base::TimeDelta time_waited) {
  DVLOG(2) << __func__ << ": frame=" << frame_index;

  if (!IsSubmitFrameExpected(frame_index))
    return;

  webxr_->TransitionFrameAnimatingToProcessing();

  TRACE_EVENT0("gpu", "ArCore SubmitFrame");

  // Draw the current camera texture to the output default framebuffer now.
  if (have_camera_image_) {
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
    ar_image_transport_->CopyCameraImageToFramebuffer(transfer_size_,
                                                      uv_transform_);
    have_camera_image_ = false;
  }

  // We're done with the camera image for this frame, start the next ARCore
  // update if we had deferred it. This will get the next frame's camera image
  // and pose in parallel while we're waiting for this frame's rendered image.
  if (pending_getframedata_) {
    std::move(pending_getframedata_).Run();
  }

  ar_image_transport_->CreateGpuFenceForSyncToken(
      sync_token, base::BindOnce(&ArCoreGl::OnWebXrTokenSignaled, GetWeakPtr(),
                                 frame_index));
}

void ArCoreGl::OnWebXrTokenSignaled(int16_t frame_index,
                                    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DVLOG(3) << __func__ << ": frame=" << frame_index;

  webxr_->TransitionFrameProcessingToRendering();

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
  ar_image_transport_->CopyDrawnImageToFramebuffer(transfer_size_,
                                                   webxr_transform_);
  surface_->SwapBuffers(base::DoNothing());
  DVLOG(3) << __func__ << ": frame=" << frame_index << " SwapBuffers";

  webxr_->EndFrameRendering();

  if (submit_client_) {
    // Create a local GpuFence and pass it to the Renderer via IPC.
    std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
    std::unique_ptr<gfx::GpuFence> gpu_fence2 = gl_fence->GetGpuFence();
    submit_client_->OnSubmitFrameGpuFence(
        gfx::CloneHandleForIPC(gpu_fence2->GetGpuFenceHandle()));
  }
}

void ArCoreGl::UpdateLayerBounds(int16_t frame_index,
                                 const gfx::RectF& left_bounds,
                                 const gfx::RectF& right_bounds,
                                 const gfx::Size& source_size) {
  DVLOG(2) << __func__;
  // Nothing to do
}

void ArCoreGl::GetEnvironmentIntegrationProvider(
    device::mojom::XREnvironmentIntegrationProviderAssociatedRequest
        environment_request) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  environment_binding_.Bind(std::move(environment_request));
  environment_binding_.set_connection_error_handler(base::BindOnce(
      &ArCoreGl::OnBindingDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

void ArCoreGl::SetInputSourceButtonListener(
    device::mojom::XRInputSourceButtonListenerAssociatedPtrInfo) {
  // Input eventing is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Input eventing is not supported.");
}

void ArCoreGl::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback) {
  DVLOG(2) << __func__ << ": ray origin=" << ray->origin.ToString()
           << ", direction=" << ray->direction.ToString();

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  if (restrict_frame_data_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::unique_ptr<ArCoreHitTestRequest> request =
      std::make_unique<ArCoreHitTestRequest>();
  request->ray = std::move(ray);
  request->callback = std::move(callback);
  hit_test_requests_.push_back(std::move(request));
}

void ArCoreGl::SetFrameDataRestricted(bool frame_data_restricted) {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  DVLOG(3) << __func__ << ": frame_data_restricted=" << frame_data_restricted;
  restrict_frame_data_ = frame_data_restricted;
  if (restrict_frame_data_) {
    Pause();
  } else {
    Resume();
  }
}

void ArCoreGl::ProcessFrame(
    mojom::XRFrameDataPtr frame_data,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  DVLOG(3) << __func__ << " frame=" << frame_data->frame_id;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  // The timing requirements for hit-test are documented here:
  // https://github.com/immersive-web/hit-test/blob/master/explainer.md#timing
  // The current implementation of frame generation on the renderer side is
  // 1:1 with calls to this method, so it is safe to fire off the hit-test
  // results here, one at a time, in the order they were enqueued prior to
  // running the GetFrameDataCallback.
  // Since mojo callbacks are processed in order, this will result in the
  // correct sequence of hit-test callbacks / promise resolutions. If
  // the implementation of the renderer processing were to change, this
  // code is fragile and could break depending on the new implementation.
  // TODO(https://crbug.com/844174): In order to be more correct by design,
  // hit results should be bundled with the frame data - that way it would be
  // obvious how the timing between the results and the frame should go.
  for (auto& request : hit_test_requests_) {
    std::vector<mojom::XRHitResultPtr> results;
    if (arcore_->RequestHitTest(request->ray, &results)) {
      std::move(request->callback).Run(std::move(results));
    } else {
      // Hit test failed, i.e. unprojected location was offscreen.
      std::move(request->callback).Run(base::nullopt);
    }
  }
  hit_test_requests_.clear();

  // Running this callback after resolving all the hit-test requests ensures
  // that we satisfy the guarantee of the WebXR hit-test spec - that the
  // hit-test promise resolves immediately prior to the frame for which it is
  // valid.
  std::move(callback).Run(std::move(frame_data));
}

void ArCoreGl::OnScreenTouch(bool touching, const gfx::PointF& touch_point) {
  DVLOG(2) << __func__ << ": touching=" << touching;
  screen_last_touch_ = touch_point;
  screen_touch_active_ = touching;
  if (touching)
    screen_touch_pending_ = true;
}

mojom::XRInputSourceStatePtr ArCoreGl::GetInputSourceState() {
  DVLOG(3) << __func__;

  // If there's no active screen touch, and no unreported past click event,
  // don't report a device.
  if (!screen_touch_pending_ && !screen_touch_active_)
    return nullptr;

  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();

  // Only one controller is supported, so the source id can be static.
  state->source_id = 1;

  state->primary_input_pressed = screen_touch_active_;
  if (!screen_touch_active_ && screen_touch_pending_) {
    state->primary_input_clicked = true;
    screen_touch_pending_ = false;
  }

  state->description = device::mojom::XRInputSourceDescription::New();

  state->description->handedness = device::mojom::XRHandedness::NONE;

  state->description->target_ray_mode = device::mojom::XRTargetRayMode::TAPPING;

  // Controller doesn't have a measured position.
  state->description->emulated_position = true;

  // The Renderer code ignores state->grip for TAPPING (screen-based) target ray
  // mode, so we don't bother filling it in here. If this does get used at
  // some point in the future, this should be set to the inverse of the
  // pose rigid transform.

  // Get a viewer-space ray from screen-space coordinates by applying the
  // inverse of the projection matrix. Z coordinate of -1 means the point will
  // be projected onto the projection matrix near plane. See also
  // third_party/blink/renderer/modules/xr/xr_view.cc's UnprojectPointer.
  gfx::Point3F touch_point(
      screen_last_touch_.x() / transfer_size_.width() * 2.f - 1.f,
      (1.f - screen_last_touch_.y() / transfer_size_.height()) * 2.f - 1.f,
      -1.f);
  DVLOG(3) << __func__ << ": touch_point=" << touch_point.ToString();
  inverse_projection_.TransformPoint(&touch_point);
  DVLOG(3) << __func__ << ": unprojected=" << touch_point.ToString();

  // Ray points along -Z in ray space, so we need to flip it to get
  // the +Z axis unit vector.
  gfx::Vector3dF ray_backwards(-touch_point.x(), -touch_point.y(),
                               -touch_point.z());
  gfx::Vector3dF new_z;
  bool can_normalize = ray_backwards.GetNormalized(&new_z);
  DCHECK(can_normalize);

  // Complete the ray-space basis by adding X and Y unit
  // vectors based on cross products.
  const gfx::Vector3dF kUp(0.f, 1.f, 0.f);
  gfx::Vector3dF new_x(kUp);
  new_x.Cross(new_z);
  new_x.GetNormalized(&new_x);
  gfx::Vector3dF new_y(new_z);
  new_y.Cross(new_x);
  new_y.GetNormalized(&new_y);

  // Fill in the transform matrix in column-major order. The first three columns
  // contain the basis vectors, the fourth column the position offset.
  gfx::Transform from_ray_space(
      new_x.x(), new_x.y(), new_x.z(), 0,  // X basis vector
      new_y.x(), new_y.y(), new_y.z(), 0,  // Y basis vector
      new_z.x(), new_z.y(), new_z.z(), 0,  // Z basis vector
      touch_point.x(), touch_point.y(), touch_point.z(), 1);
  DVLOG(3) << __func__ << ": from_ray_space=" << from_ray_space.ToString();

  // We now have a transform from ray space to viewer space, but the mojo
  // matrices go in the opposite direction, in this case it expects a transform
  // from grip matrix (== viewer space) to ray space, so we need to invert it.
  gfx::Transform to_ray_space;
  bool can_invert = from_ray_space.GetInverse(&to_ray_space);
  state->description->pointer_offset = to_ray_space;
  DCHECK(can_invert);

  return state;
}

void ArCoreGl::Pause() {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  DVLOG(1) << __func__;

  arcore_->Pause();
  is_paused_ = true;
}

void ArCoreGl::Resume() {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  DVLOG(1) << __func__;

  arcore_->Resume();
  is_paused_ = false;
}

void ArCoreGl::OnBindingDisconnect() {
  DVLOG(3) << __func__;

  CloseBindingsIfOpen();

  std::move(session_shutdown_callback_).Run();
}

void ArCoreGl::CloseBindingsIfOpen() {
  DVLOG(3) << __func__;

  environment_binding_.Close();
  frame_data_binding_.Close();
  session_controller_binding_.Close();
  presentation_binding_.Close();
}

bool ArCoreGl::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

base::WeakPtr<ArCoreGl> ArCoreGl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace device
