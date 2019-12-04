// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace paint_preview {

namespace {

const char kTestFeatureDir[] = "test_feature";

// Builds a PaintPreviewBaseService associated with |key|.
std::unique_ptr<KeyedService> BuildService(SimpleFactoryKey* key) {
  return std::make_unique<PaintPreviewBaseService>(
      key->GetPath(), kTestFeatureDir, key->IsOffTheRecord());
}

// Returns the GUID corresponding to |rfh|.
uint64_t FrameGuid(content::RenderFrameHost* rfh) {
  return static_cast<uint64_t>(rfh->GetProcess()->GetID()) << 32 |
         rfh->GetRoutingID();
}

}  // namespace

class MockPaintPreviewRecorder : public mojom::PaintPreviewRecorder {
 public:
  MockPaintPreviewRecorder() = default;
  ~MockPaintPreviewRecorder() override = default;

  void CapturePaintPreview(
      mojom::PaintPreviewCaptureParamsPtr params,
      mojom::PaintPreviewRecorder::CapturePaintPreviewCallback callback)
      override {
    {
      base::ScopedAllowBlockingForTesting scope;
      CheckParams(std::move(params));
      std::move(callback).Run(status_, std::move(response_));
    }
  }

  void SetExpectedParams(mojom::PaintPreviewCaptureParamsPtr params) {
    expected_params_ = std::move(params);
  }

  void SetResponse(mojom::PaintPreviewStatus status,
                   mojom::PaintPreviewCaptureResponsePtr response) {
    status_ = status;
    response_ = std::move(response);
  }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder>(
        std::move(handle)));
  }

 private:
  void CheckParams(mojom::PaintPreviewCaptureParamsPtr input_params) {
    // Ignore GUID and File as this is internal information not known by the
    // Keyed Service API.
    EXPECT_EQ(input_params->clip_rect, expected_params_->clip_rect);
    EXPECT_EQ(input_params->is_main_frame, expected_params_->is_main_frame);
  }

  mojom::PaintPreviewCaptureParamsPtr expected_params_;
  mojom::PaintPreviewStatus status_;
  mojom::PaintPreviewCaptureResponsePtr response_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(MockPaintPreviewRecorder);
};

// An approximation of a SimpleKeyedServiceFactory for a keyed
// PaintPerviewBaseService. This is different than a "real" version as it
// uses base::NoDestructor rather than base::Singleton.
class PaintPreviewBaseServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static PaintPreviewBaseServiceFactory* GetInstance() {
    // Use NoDestructor rather than a singleton due to lifetime behavior in
    // tests.
    static base::NoDestructor<PaintPreviewBaseServiceFactory> factory;
    return factory.get();
  }

  static PaintPreviewBaseService* GetForKey(SimpleFactoryKey* key) {
    return static_cast<PaintPreviewBaseService*>(
        GetInstance()->GetServiceForKey(key, true));
  }

  PaintPreviewBaseServiceFactory()
      : SimpleKeyedServiceFactory("PaintPreviewBaseService",
                                  SimpleDependencyManager::GetInstance()) {}
  ~PaintPreviewBaseServiceFactory() override = default;

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override {
    return BuildService(key);
  }

  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override {
    return key;
  }
};

class PaintPreviewBaseServiceTest : public content::RenderViewHostTestHarness {
 public:
  PaintPreviewBaseServiceTest() = default;
  ~PaintPreviewBaseServiceTest() override = default;

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    key_ = std::make_unique<SimpleFactoryKey>(
        browser_context()->GetPath(), browser_context()->IsOffTheRecord());
    PaintPreviewBaseServiceFactory::GetInstance()->SetTestingFactory(
        key_.get(), base::BindRepeating(&BuildService));
  }

  void OverrideInterface(MockPaintPreviewRecorder* service) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        main_rfh()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
                            base::Unretained(service)));
  }

  PaintPreviewBaseService* CreateService() {
    return PaintPreviewBaseServiceFactory::GetForKey(key_.get());
  }

 private:
  std::unique_ptr<SimpleFactoryKey> key_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PaintPreviewBaseServiceTest);
};

TEST_F(PaintPreviewBaseServiceTest, CaptureMainFrame) {
  MockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->is_main_frame = true;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->id = main_rfh()->GetRoutingID();
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk, std::move(response));
  OverrideInterface(&recorder);

  auto* service = CreateService();
  EXPECT_FALSE(service->IsOffTheRecord());
  base::FilePath path;
  ASSERT_TRUE(service->GetFileManager()->CreateOrGetDirectoryFor(
      web_contents()->GetLastCommittedURL(), &path));

  base::RunLoop loop;
  service->CapturePaintPreview(
      web_contents(), path, gfx::Rect(0, 0, 0, 0),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             PaintPreviewBaseService::CaptureStatus expected_status,
             const base::FilePath& expected_path, uint64_t expected_guid,
             PaintPreviewBaseService::CaptureStatus status,
             std::unique_ptr<PaintPreviewProto> proto) {
            EXPECT_EQ(status, expected_status);
            EXPECT_TRUE(proto->has_root_frame());
            EXPECT_EQ(proto->subframes_size(), 0);
            EXPECT_TRUE(proto->root_frame().is_main_frame());
#if defined(OS_WIN)
            base::FilePath path = base::FilePath(
                base::UTF8ToUTF16(proto->root_frame().file_path()));
#else
            base::FilePath path =
                base::FilePath(proto->root_frame().file_path());
#endif
            EXPECT_EQ(path.DirName(), expected_path);
            EXPECT_EQ(proto->root_frame().id(), expected_guid);
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(), PaintPreviewBaseService::CaptureStatus::kOk, path,
          FrameGuid(main_rfh())));
  loop.Run();
}

TEST_F(PaintPreviewBaseServiceTest, CaptureFailed) {
  MockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->is_main_frame = true;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->id = main_rfh()->GetRoutingID();
  recorder.SetResponse(mojom::PaintPreviewStatus::kFailed, std::move(response));
  OverrideInterface(&recorder);

  auto* service = CreateService();
  EXPECT_FALSE(service->IsOffTheRecord());
  base::FilePath path;
  ASSERT_TRUE(service->GetFileManager()->CreateOrGetDirectoryFor(
      web_contents()->GetLastCommittedURL(), &path));

  base::RunLoop loop;
  service->CapturePaintPreview(
      web_contents(), path, gfx::Rect(0, 0, 0, 0),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             PaintPreviewBaseService::CaptureStatus expected_status,
             PaintPreviewBaseService::CaptureStatus status,
             std::unique_ptr<PaintPreviewProto> proto) {
            EXPECT_EQ(status, expected_status);
            EXPECT_EQ(proto, nullptr);
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(),
          PaintPreviewBaseService::CaptureStatus::kCaptureFailed));
  loop.Run();
}

}  // namespace paint_preview
