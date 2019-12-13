// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/cdm_service.h"
#include "media/mojo/services/media_interface_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

namespace {

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

MATCHER_P(MatchesResult, success, "") {
  return arg->success == success;
}

const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kInvalidKeySystem[] = "invalid.key.system";
const char kSecurityOrigin[] = "https://foo.com";

class MockCdmServiceClient : public media::CdmService::Client {
 public:
  MockCdmServiceClient() = default;
  ~MockCdmServiceClient() override = default;

  // media::CdmService::Client implementation.
  MOCK_METHOD0(EnsureSandboxed, void());

  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) override {
    return std::make_unique<media::DefaultCdmFactory>();
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  void AddCdmHostFilePaths(std::vector<media::CdmHostFilePath>*) override {}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
};

class CdmServiceTest : public testing::Test {
 public:
  CdmServiceTest() = default;
  ~CdmServiceTest() override = default;

  MOCK_METHOD0(CdmServiceIdle, void());
  MOCK_METHOD0(CdmFactoryConnectionClosed, void());
  MOCK_METHOD0(CdmConnectionClosed, void());

  void Initialize() {
    auto mock_cdm_service_client = std::make_unique<MockCdmServiceClient>();
    mock_cdm_service_client_ = mock_cdm_service_client.get();

    service_ = std::make_unique<CdmService>(
        std::move(mock_cdm_service_client),
        cdm_service_remote_.BindNewPipeAndPassReceiver());
    cdm_service_remote_.set_idle_handler(
        base::TimeDelta(), base::BindRepeating(&CdmServiceTest::CdmServiceIdle,
                                               base::Unretained(this)));

    mojo::PendingRemote<service_manager::mojom::InterfaceProvider> interfaces;
    auto provider = std::make_unique<MediaInterfaceProvider>(
        interfaces.InitWithNewPipeAndPassReceiver());

    ASSERT_FALSE(cdm_factory_remote_);
    cdm_service_remote_->CreateCdmFactory(
        cdm_factory_remote_.BindNewPipeAndPassReceiver(),
        std::move(interfaces));
    cdm_service_remote_.FlushForTesting();
    ASSERT_TRUE(cdm_factory_remote_);
    cdm_factory_remote_.set_disconnect_handler(base::BindOnce(
        &CdmServiceTest::CdmFactoryConnectionClosed, base::Unretained(this)));
  }

  MOCK_METHOD3(OnCdmInitialized,
               void(mojom::CdmPromiseResultPtr result,
                    int cdm_id,
                    mojo::PendingRemote<mojom::Decryptor> decryptor));

  void InitializeCdm(const std::string& key_system, bool expected_result) {
    base::RunLoop run_loop;
    cdm_factory_remote_->CreateCdm(key_system,
                                   cdm_remote_.BindNewPipeAndPassReceiver());
    cdm_remote_.set_disconnect_handler(base::BindOnce(
        &CdmServiceTest::CdmConnectionClosed, base::Unretained(this)));
    EXPECT_CALL(*this, OnCdmInitialized(MatchesResult(expected_result), _, _))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    cdm_remote_->Initialize(
        key_system, url::Origin::Create(GURL(kSecurityOrigin)), CdmConfig(),
        base::BindOnce(&CdmServiceTest::OnCdmInitialized,
                       base::Unretained(this)));
    run_loop.Run();
  }

  void DestroyService() { service_.reset(); }

  MockCdmServiceClient* mock_cdm_service_client() {
    return mock_cdm_service_client_;
  }

  CdmService* cdm_service() { return service_.get(); }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::CdmService> cdm_service_remote_;
  mojo::Remote<mojom::CdmFactory> cdm_factory_remote_;
  mojo::Remote<mojom::ContentDecryptionModule> cdm_remote_;

 private:
  std::unique_ptr<CdmService> service_;
  MockCdmServiceClient* mock_cdm_service_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CdmServiceTest);
};

}  // namespace

TEST_F(CdmServiceTest, LoadCdm) {
  Initialize();

  // Even with a dummy path where the CDM cannot be loaded, EnsureSandboxed()
  // should still be called to ensure the process is sandboxed.
  EXPECT_CALL(*mock_cdm_service_client(), EnsureSandboxed());

  base::FilePath cdm_path(FILE_PATH_LITERAL("dummy path"));
#if defined(OS_MACOSX)
  // Token provider will not be used since the path is a dummy path.
  cdm_service_remote_->LoadCdm(cdm_path, mojo::NullRemote());
#else
  cdm_service_remote_->LoadCdm(cdm_path);
#endif

  cdm_service_remote_.FlushForTesting();
}

TEST_F(CdmServiceTest, InitializeCdm_Success) {
  Initialize();
  InitializeCdm(kClearKeyKeySystem, true);
}

TEST_F(CdmServiceTest, InitializeCdm_InvalidKeySystem) {
  Initialize();
  InitializeCdm(kInvalidKeySystem, false);
}

TEST_F(CdmServiceTest, DestroyAndRecreateCdm) {
  Initialize();
  InitializeCdm(kClearKeyKeySystem, true);
  cdm_remote_.reset();
  InitializeCdm(kClearKeyKeySystem, true);
}

// CdmFactory disconnection will cause the service to idle when no other
// interfaces are connected.
TEST_F(CdmServiceTest, DestroyCdmFactory) {
  Initialize();
  auto* service = cdm_service();

  InitializeCdm(kClearKeyKeySystem, true);
  EXPECT_EQ(service->BoundCdmFactorySizeForTesting(), 1u);
  EXPECT_EQ(service->UnboundCdmFactorySizeForTesting(), 0u);

  // Will not idle yet, because |cdm_remote_| is still connected.
  cdm_factory_remote_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(service->BoundCdmFactorySizeForTesting(), 0u);
  EXPECT_EQ(service->UnboundCdmFactorySizeForTesting(), 1u);

  // Now it should idle.
  cdm_remote_.reset();
  EXPECT_CALL(*this, CdmServiceIdle());
  base::RunLoop().RunUntilIdle();
}

// Destroy service will destroy the CdmFactory and all CDMs.
TEST_F(CdmServiceTest, DestroyCdmService) {
  Initialize();
  InitializeCdm(kClearKeyKeySystem, true);

  base::RunLoop run_loop;
  // Ideally we should not care about order, and should only quit the loop when
  // both connections are closed.
  EXPECT_CALL(*this, CdmFactoryConnectionClosed());
  EXPECT_CALL(*this, CdmConnectionClosed())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));
  DestroyService();
  run_loop.Run();
}

}  // namespace media
