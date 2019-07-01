// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_service_test_hook.h"

#include "base/bind.h"
#include "base/process/process.h"
#include "chrome/services/isolated_xr_device/xr_test_hook_wrapper.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_api_wrapper.h"
#endif  // BUILDFLAG(ENABLE_OPENVR)

#if BUILDFLAG(ENABLE_WINDOWS_MR)
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#endif  // BUILDFLAG(ENABLE_WINDOWS_MR)

namespace {

void UnsetTestHook(std::unique_ptr<device::XRTestHookWrapper> wrapper) {
#if BUILDFLAG(ENABLE_OPENVR)
  // Unset the testhook wrapper with OpenVR, so any
  // future calls to OpenVR don't use it.
  device::OpenVRWrapper::SetTestHook(nullptr);
#endif  // BUILDFLAG(ENABLE_OPENVR)

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  device::MixedRealityDeviceStatics::SetTestHook(nullptr);
#endif  // BUILDFLAG(ENABLE_WINDOWS_MR)
}

}  // namespace

namespace device {

void XRServiceTestHook::SetTestHook(
    device_test::mojom::XRTestHookPtr hook,
    device_test::mojom::XRServiceTestHook::SetTestHookCallback callback) {
  // Create a new wrapper (or use null)
  std::unique_ptr<XRTestHookWrapper> wrapper =
      hook ? std::make_unique<XRTestHookWrapper>(hook.PassInterface())
           : nullptr;

  // Register the wrapper testhook with OpenVR and WMR.
#if BUILDFLAG(ENABLE_OPENVR)
  OpenVRWrapper::SetTestHook(wrapper.get());
#endif  // BUILDFLAG(ENABLE_OPENVR)

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  MixedRealityDeviceStatics::SetTestHook(wrapper.get());
#endif  // BUILDFLAG(ENABLE_WINDOWS_MR)

  // Store the new wrapper, so we keep it alive.
  wrapper_ = std::move(wrapper);

  std::move(callback).Run();
}

void XRServiceTestHook::TerminateDeviceServiceProcessForTesting(
    DeviceCrashCallback callback) {
  base::Process::TerminateCurrentProcessImmediately(1);
}

XRServiceTestHook::~XRServiceTestHook() {
  // If we have an existing wrapper, and it is bound to a thread, post a message
  // to destroy it on that thread.
  if (wrapper_) {
    auto runner = wrapper_->GetBoundTaskRunner();
    runner->PostTask(FROM_HERE,
                     base::BindOnce(UnsetTestHook, std::move(wrapper_)));
  }
}

XRServiceTestHook::XRServiceTestHook(
    std::unique_ptr<service_manager::ServiceKeepaliveRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

}  // namespace device
