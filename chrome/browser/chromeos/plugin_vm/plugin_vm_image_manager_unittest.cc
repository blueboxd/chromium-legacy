// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_drive_image_download_service.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_download_client.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/download/public/background_service/test/test_download_service.h"
#include "components/drive/service/dummy_drive_service.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "google_apis/drive/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

namespace {

using ::testing::_;
using ::testing::AtLeast;

const char kProfileName[] = "p1";
const char kUrl[] = "http://example.com";
const char kDriveUrl[] = "https://drive.google.com/open?id=fakedriveid";
const char kDriveId[] = "fakedriveid";
const char kDriveUrl2[] = "https://drive.google.com/open?id=nonexistantdriveid";
const char kDriveContentType[] = "application/zip";
const char kPluginVmImageFile[] = "plugin_vm_image_file_1.zip";
const char kContent[] = "This is zipped content.";
const char kHash[] =
    "c80344fd4a0e9ee9f803f64edb3ea3ed8b11fe300869817e8fd50898d0663c35";
const char kHashUppercase[] =
    "C80344FD4A0E9EE9F803F64EDB3EA3ED8B11FE300869817E8FD50898D0663C35";
const char kHash2[] =
    "02f06421ae27144aacdc598aebcd345a5e2e634405e8578300173628fe1574bd";
// File size set in test_download_service.
const int kDownloadedPluginVmImageSizeInMb = 123456789u / (1024 * 1024);

}  // namespace

class MockObserver : public PluginVmImageManager::Observer {
 public:
  MOCK_METHOD0(OnDlcDownloadStarted, void());
  MOCK_METHOD2(OnDlcDownloadProgressUpdated,
               void(double progress, base::TimeDelta elapsed_time));
  MOCK_METHOD0(OnDlcDownloadCompleted, void());
  MOCK_METHOD0(OnDlcDownloadCancelled, void());
  MOCK_METHOD1(OnDlcDownloadFailed,
               void(plugin_vm::PluginVmImageManager::FailureReason));
  MOCK_METHOD0(OnDownloadStarted, void());
  MOCK_METHOD3(OnDownloadProgressUpdated,
               void(uint64_t bytes_downloaded,
                    int64_t content_length,
                    base::TimeDelta elapsed_time));
  MOCK_METHOD0(OnDownloadCompleted, void());
  MOCK_METHOD0(OnDownloadCancelled, void());
  MOCK_METHOD1(OnDownloadFailed,
               void(plugin_vm::PluginVmImageManager::FailureReason));
  MOCK_METHOD2(OnImportProgressUpdated,
               void(int percent_completed, base::TimeDelta elapsed_time));
  MOCK_METHOD0(OnImported, void());
  MOCK_METHOD0(OnImportCancelled, void());
  MOCK_METHOD1(OnImportFailed,
               void(plugin_vm::PluginVmImageManager::FailureReason));
};

// We are inheriting from DummyDriveService instead of DriveServiceInterface
// here since we are only interested in a couple of methods and don't need to
// define the rest.
class SimpleFakeDriveService : public drive::DummyDriveService {
 public:
  using DownloadActionCallback = google_apis::DownloadActionCallback;
  using GetContentCallback = google_apis::GetContentCallback;
  using ProgressCallback = google_apis::ProgressCallback;

  void RunDownloadActionCallback(google_apis::DriveApiErrorCode error,
                                 const base::FilePath& temp_file) {
    download_action_callback_.Run(error, temp_file);
  }

  void RunGetContentCallback(google_apis::DriveApiErrorCode error,
                             std::unique_ptr<std::string> content) {
    get_content_callback_.Run(error, std::move(content));
  }

  void RunProgressCallback(int64_t progress, int64_t total) {
    progress_callback_.Run(progress, total);
  }

  bool cancel_callback_called() const { return cancel_callback_called_; }

  // DriveServiceInterface override.
  google_apis::CancelCallback DownloadFile(
      const base::FilePath& /*cache_path*/,
      const std::string& /*resource_id*/,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const ProgressCallback& progress_callback) override {
    download_action_callback_ = download_action_callback;
    get_content_callback_ = get_content_callback;
    progress_callback_ = progress_callback;

    // It is safe to use base::Unretained as this object will not get deleted
    // before the end of the test.
    return base::BindRepeating(&SimpleFakeDriveService::CancelCallback,
                               base::Unretained(this));
  }

 private:
  void CancelCallback() { cancel_callback_called_ = true; }

  bool cancel_callback_called_{false};

  DownloadActionCallback download_action_callback_;
  GetContentCallback get_content_callback_;
  ProgressCallback progress_callback_;
};

class PluginVmImageManagerTestBase : public testing::Test {
 public:
  PluginVmImageManagerTestBase() = default;
  ~PluginVmImageManagerTestBase() override = default;

 protected:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    ASSERT_TRUE(profiles_dir_.CreateUniqueTempDir());
    CreateProfile();
    plugin_vm_test_helper_ =
        std::make_unique<PluginVmTestHelper>(profile_.get());
    plugin_vm_test_helper_->AllowPluginVm();
    // Sets new PluginVmImage pref for this test.
    SetPluginVmImagePref(kUrl, kHash);

    manager_ = PluginVmImageManagerFactory::GetForProfile(profile_.get());
    observer_ = std::make_unique<MockObserver>();
    manager_->SetObserver(observer_.get());

    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());

    chromeos::DlcserviceClient::InitializeFake();
    fake_dlcservice_client_ = static_cast<chromeos::FakeDlcserviceClient*>(
        chromeos::DlcserviceClient::Get());
  }

  void TearDown() override {
    observer_.reset();
    plugin_vm_test_helper_.reset();
    profile_.reset();
    observer_.reset();

    chromeos::DBusThreadManager::Shutdown();
    chromeos::DlcserviceClient::Shutdown();
  }

  void SetPluginVmImagePref(std::string url, std::string hash) {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                plugin_vm::prefs::kPluginVmImage);
    base::DictionaryValue* plugin_vm_image = update.Get();
    plugin_vm_image->SetKey("url", base::Value(url));
    plugin_vm_image->SetKey("hash", base::Value(hash));
  }

  void ProcessImageUntilImporting() {
    manager_->StartDlcDownload();
    task_environment_.RunUntilIdle();
    manager_->StartDownload();
    task_environment_.RunUntilIdle();
  }

  void ProcessImageUntilConfigured() {
    ProcessImageUntilImporting();

    // Faking downloaded file for testing.
    manager_->SetDownloadedPluginVmImageArchiveForTesting(
        fake_downloaded_plugin_vm_image_archive_);
    manager_->StartImport();
    task_environment_.RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PluginVmTestHelper> plugin_vm_test_helper_;
  PluginVmImageManager* manager_;
  std::unique_ptr<MockObserver> observer_;
  base::FilePath fake_downloaded_plugin_vm_image_archive_;
  // Owned by chromeos::DBusThreadManager
  chromeos::FakeConciergeClient* fake_concierge_client_;
  chromeos::FakeDlcserviceClient* fake_dlcservice_client_;

 private:
  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileName);
    profile_builder.SetPath(profiles_dir_.GetPath().AppendASCII(kProfileName));
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    profile_builder.SetPrefService(std::move(pref_service));
    profile_ = profile_builder.Build();
  }

  base::ScopedTempDir profiles_dir_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManagerTestBase);
};

class PluginVmImageManagerDownloadServiceTest
    : public PluginVmImageManagerTestBase {
 public:
  PluginVmImageManagerDownloadServiceTest() = default;
  ~PluginVmImageManagerDownloadServiceTest() override = default;

 protected:
  void SetUp() override {
    PluginVmImageManagerTestBase::SetUp();

    download_service_ = std::make_unique<download::test::TestDownloadService>();
    download_service_->SetIsReady(true);
    download_service_->SetHash256(kHash);
    client_ = std::make_unique<PluginVmImageDownloadClient>(profile_.get());
    download_service_->set_client(client_.get());
    manager_->SetDownloadServiceForTesting(download_service_.get());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    fake_downloaded_plugin_vm_image_archive_ = CreateZipFile();
  }

  void TearDown() override {
    PluginVmImageManagerTestBase::TearDown();

    histogram_tester_.reset();
    download_service_.reset();
    client_.reset();
  }

  base::FilePath CreateZipFile() {
    base::FilePath zip_file_path =
        profile_->GetPath().AppendASCII(kPluginVmImageFile);
    base::WriteFile(zip_file_path, kContent, strlen(kContent));
    return zip_file_path;
  }

  std::unique_ptr<download::test::TestDownloadService> download_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  std::unique_ptr<PluginVmImageDownloadClient> client_;
  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManagerDownloadServiceTest);
};

class PluginVmImageManagerDriveTest : public PluginVmImageManagerTestBase {
 public:
  PluginVmImageManagerDriveTest() = default;
  ~PluginVmImageManagerDriveTest() override = default;

 protected:
  void SetUp() override {
    PluginVmImageManagerTestBase::SetUp();

    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    std::unique_ptr<google_apis::FileResource> entry;
    auto fake_drive_service = std::make_unique<drive::FakeDriveService>();
    // We will need to access this object for some tests in the future.
    fake_drive_service_ = fake_drive_service.get();
    fake_drive_service->AddNewFileWithResourceId(
        kDriveId, kDriveContentType, kContent,
        "",  // parent_resource_id
        kPluginVmImageFile,
        true,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(entry);

    auto drive_download_service =
        std::make_unique<PluginVmDriveImageDownloadService>(manager_,
                                                            profile_.get());
    // We will need to access this object for some tests in the future.
    drive_download_service_ = drive_download_service.get();

    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    drive_download_service->SetDownloadDirectoryForTesting(temp_dir.Take());
    drive_download_service->SetDriveServiceForTesting(
        std::move(fake_drive_service));

    manager_->SetDriveDownloadServiceForTesting(
        std::move(drive_download_service));
  }

  SimpleFakeDriveService* SetUpSimpleFakeDriveService() {
    auto fake_drive_service = std::make_unique<SimpleFakeDriveService>();
    SimpleFakeDriveService* fake_drive_service_ptr = fake_drive_service.get();

    drive_download_service_->SetDriveServiceForTesting(
        std::move(fake_drive_service));

    return fake_drive_service_ptr;
  }

  PluginVmDriveImageDownloadService* drive_download_service_;
  drive::FakeDriveService* fake_drive_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManagerDriveTest);
};

TEST_F(PluginVmImageManagerDownloadServiceTest,
       DownloadPluginVmImageParamsTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDlcDownloadStarted());
  EXPECT_CALL(*observer_, OnDlcDownloadCompleted());
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportProgressUpdated(50.0, _));
  EXPECT_CALL(*observer_, OnImported());

  manager_->StartDlcDownload();
  task_environment_.RunUntilIdle();
  manager_->StartDownload();

  std::string guid = manager_->GetCurrentDownloadGuidForTesting();
  base::Optional<download::DownloadParams> params =
      download_service_->GetDownload(guid);
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(guid, params->guid);
  EXPECT_EQ(download::DownloadClient::PLUGIN_VM_IMAGE, params->client);
  EXPECT_EQ(GURL(kUrl), params->request_params.url);

  // Finishing image processing.
  task_environment_.RunUntilIdle();
  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);
  manager_->StartImport();
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmImageManagerDownloadServiceTest, OnlyOneImageIsProcessedTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDlcDownloadStarted());
  EXPECT_CALL(*observer_, OnDlcDownloadCompleted());
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportProgressUpdated(50.0, _));
  EXPECT_CALL(*observer_, OnImported());

  manager_->StartDlcDownload();
  task_environment_.RunUntilIdle();
  manager_->StartDownload();

  EXPECT_TRUE(manager_->IsProcessing());

  task_environment_.RunUntilIdle();
  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);

  EXPECT_TRUE(manager_->IsProcessing());

  manager_->StartImport();

  EXPECT_TRUE(manager_->IsProcessing());

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(manager_->IsProcessing());

  histogram_tester_->ExpectUniqueSample(kPluginVmImageDownloadedSizeHistogram,
                                        kDownloadedPluginVmImageSizeInMb, 1);
}

TEST_F(PluginVmImageManagerDownloadServiceTest,
       CanProceedWithANewImageWhenSucceededTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(2);
  EXPECT_CALL(*observer_, OnImportProgressUpdated(50.0, _)).Times(2);
  EXPECT_CALL(*observer_, OnImported()).Times(2);

  ProcessImageUntilConfigured();

  EXPECT_FALSE(manager_->IsProcessing());

  // As it is deleted after successful importing.
  fake_downloaded_plugin_vm_image_archive_ = CreateZipFile();
  ProcessImageUntilConfigured();

  histogram_tester_->ExpectUniqueSample(kPluginVmImageDownloadedSizeHistogram,
                                        kDownloadedPluginVmImageSizeInMb, 2);
}

TEST_F(PluginVmImageManagerDownloadServiceTest,
       CanProceedWithANewImageWhenFailedTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(
      *observer_,
      OnDownloadFailed(
          PluginVmImageManager::FailureReason::DOWNLOAD_FAILED_ABORTED));
  EXPECT_CALL(*observer_, OnDlcDownloadStarted()).Times(2);
  EXPECT_CALL(*observer_, OnDlcDownloadCompleted()).Times(2);
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportProgressUpdated(50.0, _));
  EXPECT_CALL(*observer_, OnImported());

  manager_->StartDlcDownload();
  task_environment_.RunUntilIdle();
  manager_->StartDownload();
  std::string guid = manager_->GetCurrentDownloadGuidForTesting();
  download_service_->SetFailedDownload(guid, false);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(manager_->IsProcessing());

  ProcessImageUntilConfigured();

  histogram_tester_->ExpectUniqueSample(kPluginVmImageDownloadedSizeHistogram,
                                        kDownloadedPluginVmImageSizeInMb, 1);
}

TEST_F(PluginVmImageManagerDownloadServiceTest, CancelledDownloadTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(0);
  EXPECT_CALL(*observer_, OnDownloadCancelled());

  manager_->StartDownload();
  manager_->CancelDownload();
  task_environment_.RunUntilIdle();
  // Finishing image processing as it should really happen.
  manager_->OnDownloadCancelled();

  histogram_tester_->ExpectTotalCount(kPluginVmImageDownloadedSizeHistogram, 0);
}

TEST_F(PluginVmImageManagerDownloadServiceTest, ImportNonExistingImageTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_,
              OnImportFailed(
                  PluginVmImageManager::FailureReason::COULD_NOT_OPEN_IMAGE));

  ProcessImageUntilImporting();
  // Should fail as fake downloaded file isn't set.
  manager_->StartImport();
  task_environment_.RunUntilIdle();

  histogram_tester_->ExpectUniqueSample(kPluginVmImageDownloadedSizeHistogram,
                                        kDownloadedPluginVmImageSizeInMb, 1);
}

TEST_F(PluginVmImageManagerDownloadServiceTest, CancelledImportTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);
  SetupConciergeForCancelDiskImageOperation(fake_concierge_client_,
                                            true /* success */);

  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnImportCancelled());

  ProcessImageUntilImporting();

  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);
  manager_->StartImport();
  manager_->CancelImport();
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmImageManagerDownloadServiceTest, EmptyPluginVmImageUrlTest) {
  SetPluginVmImagePref("", kHash);
  EXPECT_CALL(
      *observer_,
      OnDownloadFailed(PluginVmImageManager::FailureReason::INVALID_IMAGE_URL));
  EXPECT_CALL(*observer_, OnDlcDownloadStarted());
  EXPECT_CALL(*observer_, OnDlcDownloadCompleted());
  ProcessImageUntilImporting();

  histogram_tester_->ExpectTotalCount(kPluginVmImageDownloadedSizeHistogram, 0);
}

TEST_F(PluginVmImageManagerDownloadServiceTest, VerifyDownloadTest) {
  EXPECT_FALSE(manager_->VerifyDownload(kHash2));
  EXPECT_TRUE(manager_->VerifyDownload(kHashUppercase));
  EXPECT_TRUE(manager_->VerifyDownload(kHash));
  EXPECT_FALSE(manager_->VerifyDownload(std::string()));
}

TEST_F(PluginVmImageManagerDownloadServiceTest,
       CannotStartDownloadIfDlcDownloadNotRun) {
  EXPECT_CALL(
      *observer_,
      OnDownloadFailed(
          PluginVmImageManager::FailureReason::DLC_DOWNLOAD_NOT_STARTED));
  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(0);
  manager_->StartDownload();
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmImageManagerDownloadServiceTest,
       CannotStartDlcDownloadIfPluginVmGetsDisabled) {
  profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      chromeos::kPluginVmAllowed, false);
  EXPECT_CALL(
      *observer_,
      OnDownloadFailed(PluginVmImageManager::FailureReason::NOT_ALLOWED));
  EXPECT_CALL(*observer_, OnDlcDownloadCompleted()).Times(0);
  manager_->StartDlcDownload();
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmImageManagerDriveTest, InvalidDriveUrlTest) {
  SetPluginVmImagePref(kDriveUrl2, kHash);

  EXPECT_CALL(*observer_, OnDownloadStarted());
  EXPECT_CALL(
      *observer_,
      OnDownloadFailed(PluginVmImageManager::FailureReason::INVALID_IMAGE_URL));
  ProcessImageUntilImporting();
}

TEST_F(PluginVmImageManagerDriveTest, NoConnectionDriveTest) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  fake_drive_service_->set_offline(true);

  EXPECT_CALL(*observer_, OnDownloadStarted());
  EXPECT_CALL(
      *observer_,
      OnDownloadFailed(
          PluginVmImageManager::FailureReason::DOWNLOAD_FAILED_NETWORK));
  ProcessImageUntilImporting();
}

TEST_F(PluginVmImageManagerDriveTest, WrongHashDriveTest) {
  SetPluginVmImagePref(kDriveUrl, kHash2);

  EXPECT_CALL(*observer_, OnDownloadStarted());
  EXPECT_CALL(
      *observer_,
      OnDownloadFailed(PluginVmImageManager::FailureReason::HASH_MISMATCH));

  ProcessImageUntilImporting();
}

TEST_F(PluginVmImageManagerDriveTest, DriveDownloadFailedAfterStartingTest) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  SimpleFakeDriveService* fake_drive_service = SetUpSimpleFakeDriveService();

  EXPECT_CALL(*observer_, OnDownloadStarted());
  EXPECT_CALL(*observer_, OnDownloadProgressUpdated(5, 100, _));
  EXPECT_CALL(*observer_, OnDownloadProgressUpdated(10, 100, _));
  EXPECT_CALL(
      *observer_,
      OnDownloadFailed(
          PluginVmImageManager::FailureReason::DOWNLOAD_FAILED_NETWORK));
  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(0);

  manager_->StartDlcDownload();
  task_environment_.RunUntilIdle();
  manager_->StartDownload();
  task_environment_.RunUntilIdle();

  fake_drive_service->RunGetContentCallback(
      google_apis::HTTP_SUCCESS, std::make_unique<std::string>("Part1"));
  fake_drive_service->RunProgressCallback(5, 100);
  fake_drive_service->RunGetContentCallback(
      google_apis::HTTP_SUCCESS, std::make_unique<std::string>("Part2"));
  fake_drive_service->RunProgressCallback(10, 100);
  fake_drive_service->RunGetContentCallback(google_apis::DRIVE_NO_CONNECTION,
                                            std::unique_ptr<std::string>());
}

TEST_F(PluginVmImageManagerDriveTest, CancelledDriveDownloadTest) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  SimpleFakeDriveService* fake_drive_service = SetUpSimpleFakeDriveService();

  EXPECT_CALL(*observer_, OnDownloadStarted());
  EXPECT_CALL(*observer_, OnDownloadProgressUpdated(5, 100, _));
  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(0);

  manager_->StartDlcDownload();
  task_environment_.RunUntilIdle();
  manager_->StartDownload();
  task_environment_.RunUntilIdle();

  fake_drive_service->RunGetContentCallback(
      google_apis::HTTP_SUCCESS, std::make_unique<std::string>("Part1"));
  fake_drive_service->RunProgressCallback(5, 100);
  manager_->CancelDownload();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(fake_drive_service->cancel_callback_called());
}

TEST_F(PluginVmImageManagerDriveTest, SuccessfulDriveDownloadTest) {
  SetPluginVmImagePref(kDriveUrl, kHash);

  EXPECT_CALL(*observer_, OnDownloadStarted());
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_,
              OnDownloadProgressUpdated(_, std::strlen(kContent), _))
      .Times(AtLeast(1));

  ProcessImageUntilImporting();
}

}  // namespace plugin_vm
