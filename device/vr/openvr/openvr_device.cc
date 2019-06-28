// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_device.h"

#include <math.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "build/build_config.h"
#include "device/vr/openvr/openvr_render_loop.h"
#include "device/vr/openvr/openvr_type_converters.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

namespace {

constexpr float kDefaultIPD = 0.06f;  // Default average IPD.
constexpr base::TimeDelta kPollingInterval =
    base::TimeDelta::FromSecondsD(0.25);

mojom::VRFieldOfViewPtr OpenVRFovToWebVRFov(vr::IVRSystem* vr_system,
                                            vr::Hmd_Eye eye) {
  auto out = mojom::VRFieldOfView::New();
  float up_tan, down_tan, left_tan, right_tan;
  vr_system->GetProjectionRaw(eye, &left_tan, &right_tan, &up_tan, &down_tan);

  // TODO(billorr): Plumb the expected projection matrix over mojo instead of
  // using angles. Up and down are intentionally swapped to account for
  // differences in expected projection matrix format for GVR and OpenVR.
  out->upDegrees = gfx::RadToDeg(atanf(down_tan));
  out->downDegrees = -gfx::RadToDeg(atanf(up_tan));
  out->leftDegrees = -gfx::RadToDeg(atanf(left_tan));
  out->rightDegrees = gfx::RadToDeg(atanf(right_tan));
  return out;
}

gfx::Transform HmdMatrix34ToTransform(const vr::HmdMatrix34_t& mat) {
  // Disable formatting so that the 4x4 matrix is more readable
  // clang-format off
  return gfx::Transform(
     mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3],
     mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3],
     mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3],
     0.0f,        0.0f,        0.0f,        1.0f
  );
  // clang-format on
}

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(vr::IVRSystem* vr_system,
                                            device::mojom::XRDeviceId id) {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->id = id;
  display_info->displayName =
      GetOpenVRString(vr_system, vr::Prop_ManufacturerName_String) + " " +
      GetOpenVRString(vr_system, vr::Prop_ModelNumber_String);
  display_info->capabilities = mojom::VRDisplayCapabilities::New();
  display_info->capabilities->hasPosition = true;
  display_info->capabilities->hasExternalDisplay = true;
  display_info->capabilities->canPresent = true;
  display_info->webvr_default_framebuffer_scale = 1.0;
  display_info->webxr_default_framebuffer_scale = 1.0;

  display_info->leftEye = mojom::VREyeParameters::New();
  display_info->rightEye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = display_info->leftEye;
  mojom::VREyeParametersPtr& right_eye = display_info->rightEye;

  left_eye->fieldOfView = OpenVRFovToWebVRFov(vr_system, vr::Eye_Left);
  right_eye->fieldOfView = OpenVRFovToWebVRFov(vr_system, vr::Eye_Right);

  vr::TrackedPropertyError error = vr::TrackedProp_Success;
  float ipd = vr_system->GetFloatTrackedDeviceProperty(
      vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserIpdMeters_Float, &error);

  if (error != vr::TrackedProp_Success)
    ipd = kDefaultIPD;

  left_eye->offset = gfx::Vector3dF(-ipd * 0.5, 0.0, 0.0);
  right_eye->offset = gfx::Vector3dF(ipd * 0.5, 0.0, 0.0);

  uint32_t width, height;
  vr_system->GetRecommendedRenderTargetSize(&width, &height);
  left_eye->renderWidth = width;
  left_eye->renderHeight = height;
  right_eye->renderWidth = left_eye->renderWidth;
  right_eye->renderHeight = left_eye->renderHeight;

  display_info->stageParameters = mojom::VRStageParameters::New();
  vr::HmdMatrix34_t mat =
      vr_system->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
  display_info->stageParameters->standingTransform =
      HmdMatrix34ToTransform(mat);

  vr::IVRChaperone* chaperone = vr::VRChaperone();
  if (chaperone) {
    chaperone->GetPlayAreaSize(&display_info->stageParameters->sizeX,
                               &display_info->stageParameters->sizeZ);
  } else {
    display_info->stageParameters->sizeX = 0.0f;
    display_info->stageParameters->sizeZ = 0.0f;
  }

  return display_info;
}


}  // namespace

OpenVRDevice::OpenVRDevice()
    : VRDeviceBase(device::mojom::XRDeviceId::OPENVR_DEVICE_ID),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      exclusive_controller_binding_(this),
      gamepad_provider_factory_binding_(this),
      compositor_host_binding_(this),
      weak_ptr_factory_(this) {
  render_loop_ = std::make_unique<OpenVRRenderLoop>();

  OnPollingEvents();
}

bool OpenVRDevice::IsHwAvailable() {
  return vr::VR_IsHmdPresent();
}

bool OpenVRDevice::IsApiAvailable() {
  return vr::VR_IsRuntimeInstalled();
}

mojom::IsolatedXRGamepadProviderFactoryPtr OpenVRDevice::BindGamepadFactory() {
  mojom::IsolatedXRGamepadProviderFactoryPtr ret;
  gamepad_provider_factory_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

mojom::XRCompositorHostPtr OpenVRDevice::BindCompositorHost() {
  mojom::XRCompositorHostPtr ret;
  compositor_host_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

OpenVRDevice::~OpenVRDevice() {
  Shutdown();
}

void OpenVRDevice::Shutdown() {
  // Wait for the render loop to stop before completing destruction. This will
  // ensure that the IVRSystem doesn't get shutdown until the render loop is no
  // longer referencing it.
  if (render_loop_ && render_loop_->IsRunning())
    render_loop_->Stop();
}

void OpenVRDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  if (!EnsureValidDisplayInfo()) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  DCHECK(options->immersive);

  if (!render_loop_->IsRunning()) {
    render_loop_->Start();

    if (!render_loop_->IsRunning()) {
      std::move(callback).Run(nullptr, nullptr);
      return;
    }

    if (provider_request_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestGamepadProvider,
                                    base::Unretained(render_loop_.get()),
                                    std::move(provider_request_)));
    }

    if (overlay_request_) {
      render_loop_->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                    base::Unretained(render_loop_.get()),
                                    std::move(overlay_request_)));
    }
  }

  // We are done using OpenVR until the presentation session ends.
  openvr_ = nullptr;

  auto my_callback =
      base::BindOnce(&OpenVRDevice::OnRequestSessionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  auto on_presentation_ended = base::BindOnce(
      &OpenVRDevice::OnPresentationEnded, weak_ptr_factory_.GetWeakPtr());

  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestSession,
                                base::Unretained(render_loop_.get()),
                                std::move(on_presentation_ended),
                                std::move(options), std::move(my_callback)));
  outstanding_session_requests_count_++;
}

void OpenVRDevice::EnsureInitialized(int render_process_id,
                                     int render_frame_id,
                                     EnsureInitializedCallback callback) {
  EnsureValidDisplayInfo();
  std::move(callback).Run();
}

bool OpenVRDevice::EnsureValidDisplayInfo() {
  // Ensure we have had a valid display_info set at least once.
  if (!have_real_display_info_) {
    DCHECK(!openvr_);
    // Initialize OpenVR.
    openvr_ = std::make_unique<OpenVRWrapper>(false /* presenting */);
    if (!openvr_->IsInitialized()) {
      openvr_ = nullptr;
      return false;
    }

    SetVRDisplayInfo(CreateVRDisplayInfo(openvr_->GetSystem(), GetId()));
    have_real_display_info_ = true;
  }
  return have_real_display_info_;
}

void OpenVRDevice::OnPresentationEnded() {
  if (!openvr_ && outstanding_session_requests_count_ == 0) {
    openvr_ = std::make_unique<OpenVRWrapper>(false /* presenting */);
    if (!openvr_->IsInitialized()) {
      openvr_ = nullptr;
      return;
    }
  }
}

void OpenVRDevice::OnRequestSessionResult(
    mojom::XRRuntime::RequestSessionCallback callback,
    bool result,
    mojom::XRSessionPtr session) {
  outstanding_session_requests_count_--;
  if (!result) {
    OnPresentationEnded();
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  OnStartPresenting();

  mojom::XRSessionControllerPtr session_controller;
  exclusive_controller_binding_.Bind(mojo::MakeRequest(&session_controller));

  // Use of Unretained is safe because the callback will only occur if the
  // binding is not destroyed.
  exclusive_controller_binding_.set_connection_error_handler(
      base::BindOnce(&OpenVRDevice::OnPresentingControllerMojoConnectionError,
                     base::Unretained(this)));

  session->display_info = display_info_.Clone();

  std::move(callback).Run(std::move(session), std::move(session_controller));
}

bool OpenVRDevice::IsAvailable() {
  return vr::VR_IsRuntimeInstalled() && vr::VR_IsHmdPresent();
}

void OpenVRDevice::GetIsolatedXRGamepadProvider(
    mojom::IsolatedXRGamepadProviderRequest provider_request) {
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestGamepadProvider,
                                  base::Unretained(render_loop_.get()),
                                  std::move(provider_request)));
  } else {
    provider_request_ = std::move(provider_request);
  }
}

void OpenVRDevice::CreateImmersiveOverlay(
    mojom::ImmersiveOverlayRequest overlay_request) {
  if (render_loop_->IsRunning()) {
    render_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&XRCompositorCommon::RequestOverlay,
                                  base::Unretained(render_loop_.get()),
                                  std::move(overlay_request)));
  } else {
    overlay_request_ = std::move(overlay_request);
  }
}

// XRSessionController
void OpenVRDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  DCHECK(false);
}

void OpenVRDevice::OnPresentingControllerMojoConnectionError() {
  render_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&XRCompositorCommon::ExitPresent,
                                base::Unretained(render_loop_.get())));
  // Don't stop the render loop here. We need to keep the gamepad provider alive
  // so that we don't lose a pending mojo gamepad_callback_.
  // TODO(https://crbug.com/875187): Alternatively, we could recreate the
  // provider on the next session, or look into why the callback gets lost.
  OnExitPresent();
  exclusive_controller_binding_.Close();
}

// Only deal with events that will cause displayInfo changes for now.
void OpenVRDevice::OnPollingEvents() {
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OpenVRDevice::OnPollingEvents,
                     weak_ptr_factory_.GetWeakPtr()),
      kPollingInterval);

  if (!openvr_)
    return;

  vr::VREvent_t event;
  bool is_changed = false;
  while (openvr_->GetSystem()->PollNextEvent(&event, sizeof(event))) {
    if (event.trackedDeviceIndex != vr::k_unTrackedDeviceIndex_Hmd &&
        event.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid) {
      continue;
    }

    switch (event.eventType) {
      case vr::VREvent_TrackedDeviceUpdated:
      case vr::VREvent_IpdChanged:
      case vr::VREvent_ChaperoneDataHasChanged:
      case vr::VREvent_ChaperoneSettingsHaveChanged:
      case vr::VREvent_ChaperoneUniverseHasChanged:
        is_changed = true;
        break;

      default:
        break;
    }
  }

  if (is_changed)
    SetVRDisplayInfo(CreateVRDisplayInfo(openvr_->GetSystem(), GetId()));
}

}  // namespace device
