// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_

#include <jni.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_base.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

namespace vr {
class MailboxToSurfaceBridge;
class ArCoreInstallUtils;
}  // namespace vr

namespace device {

class ArImageTransportFactory;
class ArCoreFactory;
class ArCoreGlThread;

class ArCoreDevice : public VRDeviceBase {
 public:
  ArCoreDevice(
      std::unique_ptr<ArCoreFactory> arcore_factory,
      std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory,
      std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_to_surface_bridge,
      std::unique_ptr<vr::ArCoreInstallUtils> arcore_install_utils);
  ArCoreDevice();
  ~ArCoreDevice() override;

  // VRDeviceBase implementation.
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

  base::WeakPtr<ArCoreDevice> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnRequestInstallArModuleResult(bool success);
  void OnRequestInstallSupportedArCoreResult(bool success);

  // VRDeviceBase implementation
  void OnMailboxBridgeReady();
  void OnArCoreGlThreadInitialized();
  void OnRequestCameraPermissionComplete(bool success);

  void OnDrawingSurfaceReady(gfx::AcceleratedWidget window,
                             display::Display::Rotation rotation,
                             const gfx::Size& frame_size);
  void OnDrawingSurfaceTouch(bool touching, const gfx::PointF& location);
  void OnDrawingSurfaceDestroyed();
  void OnSessionEnded();

  template <typename... Args>
  static void RunCallbackOnTaskRunner(
      const scoped_refptr<base::TaskRunner>& task_runner,
      base::OnceCallback<void(Args...)> callback,
      Args... args) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::forward<Args>(args)...));
  }
  template <typename... Args>
  base::OnceCallback<void(Args...)> CreateMainThreadCallback(
      base::OnceCallback<void(Args...)> callback) {
    return base::BindOnce(&ArCoreDevice::RunCallbackOnTaskRunner<Args...>,
                          main_thread_task_runner_, std::move(callback));
  }

  void PostTaskToGlThread(base::OnceClosure task);

  bool IsOnMainThread();

  void RequestSessionAfterInitialization(int render_process_id,
                                         int render_frame_id);
  void RequestArModule(int render_process_id, int render_frame_id);
  void OnRequestArModuleResult(int render_process_id,
                               int render_frame_id,
                               bool success);
  void RequestArCoreInstallOrUpdate(int render_process_id, int render_frame_id);
  void OnRequestArCoreInstallOrUpdateResult(bool success);
  void CallDeferredRequestSessionCallback(bool success);
  void OnRequestAndroidCameraPermissionResult(
      base::OnceCallback<void(bool)> callback,
      bool was_android_camera_permission_granted);
  void RequestArCoreGlInitialization(gfx::AcceleratedWidget window,
                                     int rotation,
                                     const gfx::Size& size);
  void OnArCoreGlInitializationComplete(bool success);
  void RequestArSessionConsent(int render_process_id, int render_frame_id);

  void OnCreateSessionCallback(
      mojom::XRRuntime::RequestSessionCallback deferred_callback,
      mojom::XRFrameDataProviderPtrInfo frame_data_provider_info,
      mojom::VRDisplayInfoPtr display_info,
      mojom::XRSessionControllerPtrInfo session_controller_info,
      mojom::XRPresentationConnectionPtr presentation_connection);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<ArCoreFactory> arcore_factory_;
  std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory_;
  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;
  std::unique_ptr<vr::ArCoreInstallUtils> arcore_install_utils_;

  // Encapsulates data with session lifetime.
  struct SessionState {
    SessionState();
    ~SessionState();

    std::unique_ptr<ArCoreGlThread> arcore_gl_thread_;
    bool is_arcore_gl_thread_initialized_ = false;
    bool is_arcore_gl_initialized_ = false;

    base::OnceClosure start_immersive_activity_callback_;

    // The initial requestSession triggers the initialization sequence, store
    // the callback for replying once that initialization completes. Only one
    // concurrent session is supported, other requests are rejected.
    mojom::XRRuntime::RequestSessionCallback pending_request_session_callback_;

    base::OnceClosure pending_request_session_after_gl_thread_initialized_;
  };

  // This object is reset to initial values when ending a session. This helps
  // ensure that each session has consistent per-session state.
  std::unique_ptr<SessionState> session_state_;

  base::OnceCallback<void(bool)>
      on_request_arcore_install_or_update_result_callback_;
  base::OnceCallback<void(bool)> on_request_ar_module_result_callback_;

  // Must be last.
  base::WeakPtrFactory<ArCoreDevice> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArCoreDevice);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_
