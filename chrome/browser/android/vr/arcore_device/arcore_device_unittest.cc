// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_device.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/android/vr/arcore_device/ar_image_transport.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"
#include "chrome/browser/android/vr/arcore_device/arcore_install_utils.h"
#include "chrome/browser/android/vr/arcore_device/fake_arcore.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/test/fake_vr_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class StubArImageTransport : public ArImageTransport {
 public:
  explicit StubArImageTransport(
      std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge)
      : ArImageTransport(std::move(mailbox_bridge)) {}

  // TODO(lincolnfrog): verify this gets called on GL thread.
  // TODO(lincolnfrog): test what happens if this returns false.
  bool Initialize(vr::WebXrPresentationState*) override { return true; }

  // TODO(lincolnfrog): test verify this somehow.
  GLuint GetCameraTextureId() override { return CAMERA_TEXTURE_ID; }

  // This transfers whatever the contents of the texture specified
  // by GetCameraTextureId() is at the time it is called and returns
  // a gpu::MailboxHolder with that texture copied to a shared buffer.
  gpu::MailboxHolder TransferFrame(
      const gfx::Size& frame_size,
      const gfx::Transform& uv_transform) override {
    return gpu::MailboxHolder();
  }

  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;
  const GLuint CAMERA_TEXTURE_ID = 10;
};

class StubArImageTransportFactory : public ArImageTransportFactory {
 public:
  ~StubArImageTransportFactory() override = default;

  std::unique_ptr<ArImageTransport> Create(
      std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge) override {
    return std::make_unique<StubArImageTransport>(std::move(mailbox_bridge));
  }
};

class StubMailboxToSurfaceBridge : public vr::MailboxToSurfaceBridge {
 public:
  StubMailboxToSurfaceBridge() = default;

  MOCK_METHOD1(DoCreateUnboundContextProvider,
               void(base::OnceClosure callback));
  void CreateUnboundContextProvider(base::OnceClosure callback) override {
    callback_ = std::move(callback);
  }

  void BindContextProviderToCurrentThread() override {}

  bool IsConnected() override { return true; }

  void CallCallback() { std::move(callback_).Run(); }

  const uint32_t TEXTURE_ID = 1;

 private:
  base::OnceClosure callback_;
};

class StubArCoreInstallUtils : public vr::ArCoreInstallUtils {
 public:
  StubArCoreInstallUtils() = default;

  bool CanRequestInstallArModule() override { return false; }
  bool ShouldRequestInstallArModule() override { return false; }

  void RequestInstallArModule(int render_process_id,
                              int render_frame_id) override {}
  bool ShouldRequestInstallSupportedArCore() override { return false; }
  void RequestInstallSupportedArCore(int render_process_id,
                                     int render_frame_id) override {}
  void RequestArSession(
      int render_process_id,
      int render_frame_id,
      vr::SurfaceReadyCallback ready_callback,
      vr::SurfaceTouchCallback touch_callback,
      vr::SurfaceDestroyedCallback destroyed_callback) override {
    // Return arbitrary screen geometry as stand-in for the expected
    // drawing surface. It's not actually a surface, hence the nullptr
    // instead of a WindowAndroid.
    std::move(ready_callback)
        .Run(nullptr, display::Display::Rotation::ROTATE_0, {1024, 512});
  }
  void DestroyDrawingSurface() override {}

  bool EnsureLoaded() override { return true; }

  base::android::ScopedJavaLocalRef<jobject> GetApplicationContext() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread =
        env->GetStaticMethodID(activityThread, "currentActivityThread",
                               "()Landroid/app/ActivityThread;");
    jobject at =
        env->CallStaticObjectMethod(activityThread, currentActivityThread);
    jmethodID getApplication = env->GetMethodID(
        activityThread, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(at, getApplication);
    return base::android::ScopedJavaLocalRef<jobject>(env, context);
  }
};

class ArCoreDeviceTest : public testing::Test {
 public:
  ArCoreDeviceTest() {}
  ~ArCoreDeviceTest() override {}

  void OnSessionCreated(mojom::XRSessionPtr session,
                        mojom::XRSessionControllerPtr controller) {
    DVLOG(1) << __func__;
    session_ = std::move(session);
    controller_ = std::move(controller);
    // TODO(crbug.com/837834): verify that things fail if restricted.
    // We should think through the right result here for javascript.
    // If an AR page tries to hittest while not focused, should it
    // get no results or fail?
    controller_->SetFrameDataRestricted(false);

    frame_provider.Bind(std::move(session_->data_provider));
    frame_provider->GetEnvironmentIntegrationProvider(
        mojo::MakeRequest(&environment_provider));
    std::move(quit_closure).Run();
  }

  StubMailboxToSurfaceBridge* bridge;
  StubArCoreInstallUtils* install_utils;
  mojom::XRFrameDataProviderPtr frame_provider;
  mojom::XREnvironmentIntegrationProviderAssociatedPtr environment_provider;
  std::unique_ptr<base::RunLoop> run_loop;
  base::OnceClosure quit_closure;

 protected:
  void SetUp() override {
    std::unique_ptr<StubMailboxToSurfaceBridge> bridge_ptr =
        std::make_unique<StubMailboxToSurfaceBridge>();
    bridge = bridge_ptr.get();
    std::unique_ptr<StubArCoreInstallUtils> install_utils_ptr =
        std::make_unique<StubArCoreInstallUtils>();
    install_utils = install_utils_ptr.get();
    device_ = std::make_unique<ArCoreDevice>(
        std::make_unique<FakeArCoreFactory>(),
        std::make_unique<StubArImageTransportFactory>(), std::move(bridge_ptr),
        std::move(install_utils_ptr));
  }

  void CreateSession() {
    mojom::XRRuntimeSessionOptionsPtr options =
        mojom::XRRuntimeSessionOptions::New();
    options->environment_integration = true;
    options->immersive = true;
    device()->RequestSession(std::move(options),
                             base::BindOnce(&ArCoreDeviceTest::OnSessionCreated,
                                            base::Unretained(this)));

    // TODO(https://crbug.com/837834): figure out how to make this work
    // EXPECT_CALL(*bridge,
    // DoCreateUnboundContextProvider(testing::_)).Times(1);

    run_loop = std::make_unique<base::RunLoop>();
    quit_closure = run_loop->QuitClosure();
    bridge->CallCallback();
    run_loop->Run();

    EXPECT_TRUE(environment_provider);
    EXPECT_TRUE(session_);
  }

  mojom::XRFrameDataPtr GetFrameData() {
    run_loop = std::make_unique<base::RunLoop>();
    quit_closure = run_loop->QuitClosure();

    mojom::XRFrameDataPtr frame_data;
    auto callback = [](base::OnceClosure quit_closure,
                       mojom::XRFrameDataPtr* frame_data,
                       mojom::XRFrameDataPtr data) {
      *frame_data = std::move(data);
      std::move(quit_closure).Run();
    };

    // TODO(https://crbug.com/837834): verify GetFrameData fails if we
    // haven't resolved the Mailbox.
    frame_provider->GetFrameData(
        nullptr,
        base::BindOnce(callback, std::move(quit_closure), &frame_data));
    run_loop->Run();
    EXPECT_TRUE(frame_data);

    return frame_data;
  }

  VRDeviceBase* device() { return device_.get(); }

 private:
  std::unique_ptr<ArCoreDevice> device_;
  mojom::XRSessionPtr session_;
  mojom::XRSessionControllerPtr controller_;
};

TEST_F(ArCoreDeviceTest, RequestSession) {
  CreateSession();
}

TEST_F(ArCoreDeviceTest, GetFrameData) {
  CreateSession();
  GetFrameData();
}

TEST_F(ArCoreDeviceTest, RequestHitTest) {
  CreateSession();

  mojom::XRRayPtr ray = mojom::XRRay::New();
  std::vector<mojom::XRHitResultPtr> hit_results;
  auto callback =
      [](std::vector<mojom::XRHitResultPtr>* hit_results,
         base::Optional<std::vector<mojom::XRHitResultPtr>> results) {
        *hit_results = std::move(results.value());
      };

  environment_provider->RequestHitTest(std::move(ray),
                                       base::BindOnce(callback, &hit_results));
  // Have to get frame data to trigger the hit-test calculation.
  GetFrameData();
  EXPECT_TRUE(hit_results.size() > 0);
}

}  // namespace device
