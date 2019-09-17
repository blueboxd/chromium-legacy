// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_loader_factory.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/drive/chromeos/fake_file_system.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

constexpr char kExtensionId[] = "abc";
constexpr char kFileSystemId[] = "test-filesystem";
constexpr char kTestUrl[] =
    "externalfile:abc:test-filesystem:test-user-hash/hello.txt";
constexpr char kExpectedFileContents[] =
    "This is a testing file. Lorem ipsum dolor sit amet est.";

}  // namespace

class ExternalFileURLLoaderFactoryTest : public testing::Test {
 protected:
  ExternalFileURLLoaderFactoryTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        integration_service_factory_callback_(base::BindRepeating(
            &ExternalFileURLLoaderFactoryTest::CreateDriveIntegrationService,
            base::Unretained(this))),
        fake_file_system_(nullptr) {}

  ~ExternalFileURLLoaderFactoryTest() override {}

  void SetUp() override {
    // Create a testing profile.
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    Profile* const profile =
        profile_manager_->CreateTestingProfile("test-user");
    user_manager_ = std::make_unique<chromeos::FakeChromeUserManager>();
    user_manager_->AddUser(
        AccountId::FromUserEmailGaiaId(profile->GetProfileUserName(), "12345"));
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(profile);

    auto* service = chromeos::file_system_provider::Service::Get(profile);
    service->RegisterProvider(
        chromeos::file_system_provider::FakeExtensionProvider::Create(
            kExtensionId));
    const auto kProviderId =
        chromeos::file_system_provider::ProviderId::CreateFromExtensionId(
            kExtensionId);
    service->MountFileSystem(kProviderId,
                             chromeos::file_system_provider::MountOptions(
                                 kFileSystemId, "Test FileSystem"));

    // Create the drive integration service for the profile.
    integration_service_factory_scope_.reset(
        new drive::DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &integration_service_factory_callback_));
    drive::DriveIntegrationServiceFactory::GetForProfile(profile);

    // Create the URLLoaderFactory.
    url_loader_factory_ = std::make_unique<ExternalFileURLLoaderFactory>(
        profile, render_process_host_id());
  }

  virtual int render_process_host_id() {
    return content::ChildProcessHost::kInvalidUniqueID;
  }

  content::MockRenderProcessHost* render_process_host() {
    return render_process_host_.get();
  }

  network::ResourceRequest CreateRequest(std::string url) {
    network::ResourceRequest request;
    request.method = "GET";
    request.url = GURL(url);
    return request;
  }

  network::mojom::URLLoaderPtr CreateURLLoaderAndStart(
      network::TestURLLoaderClient* client,
      const network::ResourceRequest& resource_request) {
    network::mojom::URLLoaderPtr loader;
    url_loader_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&loader), 0 /* routing_id */, 0 /* request_id */,
        network::mojom::kURLLoadOptionNone, resource_request,
        client->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return loader;
  }

 private:
  // Create the drive integration service for the |profile|
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    drive::FakeDriveService* const drive_service = new drive::FakeDriveService;
    if (!drive::test_util::SetUpTestEntries(drive_service))
      return NULL;

    const std::string& drive_mount_name =
        drive::util::GetDriveMountPointPath(profile).BaseName().AsUTF8Unsafe();
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        drive_mount_name, storage::kFileSystemTypeDrive,
        storage::FileSystemMountOption(),
        drive::util::GetDriveMountPointPath(profile));
    DCHECK(!fake_file_system_);
    fake_file_system_ = new drive::test_util::FakeFileSystem(drive_service);
    if (!drive_cache_dir_.CreateUniqueTempDir())
      return NULL;
    return new drive::DriveIntegrationService(
        profile, nullptr, drive_service, drive_mount_name,
        drive_cache_dir_.GetPath(), fake_file_system_);
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestServiceManagerContext context_;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      integration_service_factory_callback_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      integration_service_factory_scope_;
  drive::test_util::FakeFileSystem* fake_file_system_;

  std::unique_ptr<ExternalFileURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<chromeos::FakeChromeUserManager> user_manager_;
  // Used to register the profile with the ChildProcessSecurityPolicyImpl.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
  base::ScopedTempDir drive_cache_dir_;
};

TEST_F(ExternalFileURLLoaderFactoryTest, NonGetMethod) {
  network::TestURLLoaderClient client;
  network::ResourceRequest request = CreateRequest(kTestUrl);
  request.method = "POST";
  network::mojom::URLLoaderPtr loader =
      CreateURLLoaderAndStart(&client, request);

  client.RunUntilComplete();

  EXPECT_EQ(net::ERR_METHOD_NOT_SUPPORTED,
            client.completion_status().error_code);
}

TEST_F(ExternalFileURLLoaderFactoryTest, RegularFile) {
  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader =
      CreateURLLoaderAndStart(&client, CreateRequest(kTestUrl));

  client.RunUntilComplete();

  ASSERT_EQ(net::OK, client.completion_status().error_code);
  EXPECT_EQ("text/plain", client.response_head().mime_type);
  std::string response_body;
  ASSERT_TRUE(mojo::BlockingCopyToString(client.response_body_release(),
                                         &response_body));
  EXPECT_EQ(kExpectedFileContents, response_body);
}

TEST_F(ExternalFileURLLoaderFactoryTest, RootDirectory) {
  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader = CreateURLLoaderAndStart(
      &client,
      CreateRequest("externalfile:abc:test-filesystem:test-user-hash/"));

  client.RunUntilComplete();

  EXPECT_EQ(net::ERR_FAILED, client.completion_status().error_code);
}

TEST_F(ExternalFileURLLoaderFactoryTest, NonExistingFile) {
  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader = CreateURLLoaderAndStart(
      &client, CreateRequest("externalfile:abc:test-filesystem:test-user-hash/"
                             "non-existing-file.txt"));

  client.RunUntilComplete();

  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client.completion_status().error_code);
}

TEST_F(ExternalFileURLLoaderFactoryTest, WrongFormat) {
  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader =
      CreateURLLoaderAndStart(&client, CreateRequest("externalfile:"));

  client.RunUntilComplete();

  EXPECT_EQ(net::ERR_INVALID_URL, client.completion_status().error_code);
}

TEST_F(ExternalFileURLLoaderFactoryTest, RangeHeader) {
  network::TestURLLoaderClient client;
  network::ResourceRequest request = CreateRequest(kTestUrl);
  request.headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=3-5");
  network::mojom::URLLoaderPtr loader =
      CreateURLLoaderAndStart(&client, request);

  client.RunUntilComplete();

  EXPECT_EQ(net::OK, client.completion_status().error_code);
  std::string response_body;
  ASSERT_TRUE(mojo::BlockingCopyToString(client.response_body_release(),
                                         &response_body));
  EXPECT_EQ(base::StringPiece(kExpectedFileContents).substr(3, 3),
            response_body);
}

TEST_F(ExternalFileURLLoaderFactoryTest, WrongRangeHeader) {
  network::TestURLLoaderClient client;
  network::ResourceRequest request = CreateRequest(kTestUrl);
  request.headers.SetHeader(net::HttpRequestHeaders::kRange, "Invalid range");
  network::mojom::URLLoaderPtr loader =
      CreateURLLoaderAndStart(&client, request);

  client.RunUntilComplete();

  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            client.completion_status().error_code);
}

class SubresourceExternalFileURLLoaderFactoryTest
    : public ExternalFileURLLoaderFactoryTest {
 protected:
  int render_process_host_id() override {
    return render_process_host()->GetID();
  }
};

TEST_F(SubresourceExternalFileURLLoaderFactoryTest, SubresourceAllowed) {
  content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      render_process_host_id(), content::kExternalFileScheme);

  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader =
      CreateURLLoaderAndStart(&client, CreateRequest(kTestUrl));

  client.RunUntilComplete();

  ASSERT_EQ(net::OK, client.completion_status().error_code);
}

TEST_F(SubresourceExternalFileURLLoaderFactoryTest, SubresourceNotAllowed) {
  network::TestURLLoaderClient client;
  ASSERT_DEATH(CreateURLLoaderAndStart(&client, CreateRequest(kTestUrl)), "");
}

}  // namespace chromeos
