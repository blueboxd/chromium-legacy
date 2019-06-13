// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include <memory>
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

namespace {
const char kVmName[] = "vm_name";
const char kContainerName[] = "container_name";
const char kPackageID[] = "package;1;;";
constexpr int64_t kDiskSizeBytes = 4ll * 1024 * 1024 * 1024;  // 4 GiB
const uint8_t kUsbPort = 0x01;
}  // namespace

class CrostiniManagerTest : public testing::Test {
 public:
  void CreateDiskImageClientErrorCallback(
      base::OnceClosure closure,
      CrostiniResult result,
      vm_tools::concierge::DiskImageStatus status,
      const base::FilePath& file_path) {
    EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    EXPECT_EQ(status,
              vm_tools::concierge::DiskImageStatus::DISK_STATUS_UNKNOWN);
    std::move(closure).Run();
  }

  void DestroyDiskImageClientErrorCallback(base::OnceClosure closure,
                                           CrostiniResult result) {
    EXPECT_FALSE(fake_concierge_client_->destroy_disk_image_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void ListVmDisksClientErrorCallback(base::OnceClosure closure,
                                      CrostiniResult result,
                                      int64_t total_size) {
    EXPECT_FALSE(fake_concierge_client_->list_vm_disks_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StartTerminaVmClientErrorCallback(base::OnceClosure closure,
                                         CrostiniResult result) {
    EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StopVmClientErrorCallback(base::OnceClosure closure,
                                 CrostiniResult result) {
    EXPECT_FALSE(fake_concierge_client_->stop_vm_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void CreateDiskImageSuccessCallback(
      base::OnceClosure closure,
      CrostiniResult result,
      vm_tools::concierge::DiskImageStatus status,
      const base::FilePath& file_path) {
    EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
    std::move(closure).Run();
  }

  void DestroyDiskImageSuccessCallback(base::OnceClosure closure,
                                       CrostiniResult result) {
    EXPECT_TRUE(fake_concierge_client_->destroy_disk_image_called());
    std::move(closure).Run();
  }

  void ListVmDisksSuccessCallback(base::OnceClosure closure,
                                  CrostiniResult result,
                                  int64_t total_size) {
    EXPECT_TRUE(fake_concierge_client_->list_vm_disks_called());
    std::move(closure).Run();
  }

  void StartTerminaVmSuccessCallback(base::OnceClosure closure,
                                     CrostiniResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
    std::move(closure).Run();
  }

  void OnStartTremplinRecordsRunningVmCallback(base::OnceClosure closure,
                                               CrostiniResult result) {
    // Check that running_vms_ contains the running vm.
    EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
    std::move(closure).Run();
  }

  void StopVmSuccessCallback(base::OnceClosure closure, CrostiniResult result) {
    EXPECT_TRUE(fake_concierge_client_->stop_vm_called());
    std::move(closure).Run();
  }

  void CreateContainerFailsCallback(base::OnceClosure closure,
                                    CrostiniResult result) {
    create_container_fails_callback_called_ = true;
    EXPECT_EQ(result, CrostiniResult::UNKNOWN_ERROR);
    std::move(closure).Run();
  }

  void CrostiniResultCallback(base::OnceClosure closure,
                              CrostiniResult expected_result,
                              CrostiniResult result) {
    EXPECT_EQ(expected_result, result);
    std::move(closure).Run();
  }

  base::ScopedFD TestFileDescriptor() {
    base::File file(base::FilePath("/dev/null"),
                    base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    base::ScopedFD fd(file.TakePlatformFile());
    return fd;
  }

  void AttachUsbDeviceCallback(base::OnceClosure closure,
                               CrostiniResult expected_result,
                               uint8_t guest_port,
                               CrostiniResult result) {
    EXPECT_TRUE(fake_concierge_client_->attach_usb_device_called());
    EXPECT_EQ(expected_result, result);
    std::move(closure).Run();
  }

  void DetachUsbDeviceCallback(base::OnceClosure closure,
                               bool expected_called,
                               CrostiniResult expected_result,
                               CrostiniResult result) {
    EXPECT_EQ(fake_concierge_client_->detach_usb_device_called(),
              expected_called);
    EXPECT_EQ(expected_result, result);
    std::move(closure).Run();
  }

  void ListUsbDevicesCallback(
      base::OnceClosure closure,
      CrostiniResult expected_result,
      size_t expected_size,
      CrostiniResult result,
      std::vector<std::pair<std::string, uint8_t>> devices) {
    EXPECT_TRUE(fake_concierge_client_->list_usb_devices_called());
    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(devices.size(), expected_size);
    std::move(closure).Run();
  }

  void SearchAppCallback(base::OnceClosure closure,
                         const std::vector<std::string>& expected_result,
                         const std::vector<std::string>& result) {
    EXPECT_THAT(result, testing::ContainerEq(expected_result));
    std::move(closure).Run();
  }

  void GetLinuxPackageInfoFromAptCallback(
      base::OnceClosure closure,
      const LinuxPackageInfo& expected_result,
      const LinuxPackageInfo& result) {
    EXPECT_EQ(result.success, expected_result.success);
    EXPECT_EQ(result.failure_reason, expected_result.failure_reason);
    EXPECT_EQ(result.package_id, expected_result.package_id);
    EXPECT_EQ(result.name, expected_result.name);
    EXPECT_EQ(result.version, expected_result.version);
    EXPECT_EQ(result.summary, expected_result.summary);
    EXPECT_EQ(result.description, expected_result.description);
    std::move(closure).Run();
  }

  CrostiniManagerTest()
      : test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {
    chromeos::DBusThreadManager::Initialize();
    fake_cicerone_client_ = static_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }

  ~CrostiniManagerTest() override { chromeos::DBusThreadManager::Shutdown(); }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kCrostini);
    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = std::make_unique<CrostiniManager>(profile_.get());

    // Login user for crostini, link gaia for DriveFS.
    auto user_manager = std::make_unique<chromeos::FakeChromeUserManager>();
    AccountId account_id = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "12345");
    user_manager->AddUser(account_id);
    user_manager->LoginUser(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    device::mojom::UsbDeviceManagerPtr fake_usb_manager_ptr_;
    fake_usb_manager_.AddBinding(mojo::MakeRequest(&fake_usb_manager_ptr_));
  }

  void TearDown() override {
    crostini_manager_.reset();
    scoped_user_manager_.reset();
    profile_.reset();
    run_loop_.reset();
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  CrostiniManager* crostini_manager() { return crostini_manager_.get(); }

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeCiceroneClient* fake_cicerone_client_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

  std::unique_ptr<base::RunLoop>
      run_loop_;  // run_loop_ must be created on the UI thread.
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniManager> crostini_manager_;
  bool create_container_fails_callback_called_ = false;
  device::FakeUsbDeviceManager fake_usb_manager_;

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniManagerTest);
};

TEST_F(CrostiniManagerTest, CreateDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");

  crostini_manager()->CreateDiskImage(
      disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT, kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageStorageLocationError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->CreateDiskImage(
      disk_path,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->CreateDiskImage(
      disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT, kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");

  crostini_manager()->DestroyDiskImage(
      disk_path,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->DestroyDiskImage(
      disk_path,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ListVmDisksSuccess) {
  crostini_manager()->ListVmDisks(
      base::BindOnce(&CrostiniManagerTest::ListVmDisksSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmNameError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->StartTerminaVm(
      "", disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmDiskPathError) {
  const base::FilePath& disk_path = base::FilePath();

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, OnStartTremplinRecordsRunningVm) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  const std::string owner_id = CryptohomeIdForProfile(profile());

  // Start the Vm.
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(
          &CrostiniManagerTest::OnStartTremplinRecordsRunningVmCallback,
          base::Unretained(this), run_loop()->QuitClosure()));

  // Check that the Vm start is not recorded (without tremplin start).
  EXPECT_FALSE(crostini_manager()->IsVmRunning(kVmName));

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StopVmNameError) {
  crostini_manager()->StopVm(
      "", base::BindOnce(&CrostiniManagerTest::StopVmClientErrorCallback,
                         base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StopVmSuccess) {
  crostini_manager()->StopVm(
      kVmName,
      base::BindOnce(&CrostiniManagerTest::StopVmSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalFailure) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  std::string failure_reason = "Unit tests can't install Linux packages!";
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason(failure_reason);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalOperationBlocked) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalNotConnectedError) {
  fake_cicerone_client_->set_uninstall_package_progress_signal_connected(false);
  crostini_manager()->UninstallPackageOwningFile(
      kVmName, kContainerName, "emacs",
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::UNINSTALL_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalSuccess) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::STARTED);
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      kVmName, kContainerName, "emacs",
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalFailure) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::FAILED);
  response.set_failure_reason("Didn't feel like it");
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      kVmName, kContainerName, "emacs",
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::UNINSTALL_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalOperationBlocked) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(vm_tools::cicerone::UninstallPackageOwningFileResponse::
                          BLOCKING_OPERATION_IN_PROGRESS);
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      kVmName, kContainerName, "emacs",
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, AttachUsbDeviceSuccess) {
  vm_tools::concierge::AttachUsbDeviceResponse response;
  response.set_success(true);
  fake_concierge_client_->set_attach_usb_device_response(response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
  fake_usb_manager_.RemoveDevice(guid);
}

TEST_F(CrostiniManagerTest, AttachUsbDeviceFailure) {
  vm_tools::concierge::AttachUsbDeviceResponse response;
  response.set_success(false);
  fake_concierge_client_->set_attach_usb_device_response(response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::ATTACH_USB_FAILED));
  run_loop()->Run();
  fake_usb_manager_.RemoveDevice(guid);
}

TEST_F(CrostiniManagerTest, DetachUsbDeviceSuccess) {
  vm_tools::concierge::AttachUsbDeviceResponse attach_response;
  attach_response.set_success(true);
  fake_concierge_client_->set_attach_usb_device_response(attach_response);

  vm_tools::concierge::DetachUsbDeviceResponse detach_response;
  detach_response.set_success(true);
  fake_concierge_client_->set_detach_usb_device_response(detach_response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  auto detach_usb = base::BindOnce(
      &CrostiniManager::DetachUsbDevice, base::Unretained(crostini_manager()),
      kVmName, fake_usb.Clone(), kUsbPort,
      base::BindOnce(&CrostiniManagerTest::DetachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(), true,
                     CrostiniResult::SUCCESS));

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), std::move(detach_usb),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
  fake_usb_manager_.RemoveDevice(guid);
}

TEST_F(CrostiniManagerTest, DetachUsbDeviceFailure) {
  vm_tools::concierge::AttachUsbDeviceResponse attach_response;
  attach_response.set_success(true);
  fake_concierge_client_->set_attach_usb_device_response(attach_response);

  vm_tools::concierge::DetachUsbDeviceResponse detach_response;
  detach_response.set_success(false);
  fake_concierge_client_->set_detach_usb_device_response(detach_response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  auto detach_usb = base::BindOnce(
      &CrostiniManager::DetachUsbDevice, base::Unretained(crostini_manager()),
      kVmName, fake_usb.Clone(), kUsbPort,
      base::BindOnce(&CrostiniManagerTest::DetachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(), true,
                     CrostiniResult::DETACH_USB_FAILED));

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), std::move(detach_usb),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
  fake_usb_manager_.RemoveDevice(guid);
}

TEST_F(CrostiniManagerTest, ListUsbDeviceFailure) {
  vm_tools::concierge::ListUsbDeviceResponse response;
  response.set_success(false);
  fake_concierge_client_->set_list_usb_devices_response(response);

  crostini_manager()->ListUsbDevices(
      kVmName, base::BindOnce(&CrostiniManagerTest::ListUsbDevicesCallback,
                              base::Unretained(this), run_loop()->QuitClosure(),
                              CrostiniResult::LIST_USB_FAILED, 0));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ListUsbDeviceEmptySuccess) {
  vm_tools::concierge::ListUsbDeviceResponse response;
  response.set_success(true);
  fake_concierge_client_->set_list_usb_devices_response(response);

  crostini_manager()->ListUsbDevices(
      kVmName, base::BindOnce(&CrostiniManagerTest::ListUsbDevicesCallback,
                              base::Unretained(this), run_loop()->QuitClosure(),
                              CrostiniResult::SUCCESS, 0));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ListUsbDeviceOne) {
  vm_tools::concierge::AttachUsbDeviceResponse attach_response;
  attach_response.set_success(true);
  attach_response.set_guest_port(1);
  fake_concierge_client_->set_attach_usb_device_response(attach_response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();

  vm_tools::concierge::ListUsbDeviceResponse response;
  response.set_success(true);
  auto* msg = response.add_usb_devices();
  msg->set_guest_port(1);
  fake_concierge_client_->set_list_usb_devices_response(response);

  base::RunLoop run_loop2;
  crostini_manager()->ListUsbDevices(
      kVmName, base::BindOnce(&CrostiniManagerTest::ListUsbDevicesCallback,
                              base::Unretained(this), run_loop2.QuitClosure(),
                              CrostiniResult::SUCCESS, 1));
  run_loop2.Run();

  fake_usb_manager_.RemoveDevice(guid);
}

class CrostiniManagerRestartTest : public CrostiniManagerTest,
                                   public CrostiniManager::RestartObserver {
 public:
  void RestartCrostiniCallback(base::OnceClosure closure,
                               CrostiniResult result) {
    restart_crostini_callback_count_++;
    std::move(closure).Run();
  }

  void RemoveCrostiniCallback(base::OnceClosure closure,
                              CrostiniResult result) {
    remove_crostini_callback_count_++;
    std::move(closure).Run();
  }

  // CrostiniManager::RestartObserver
  void OnComponentLoaded(CrostiniResult result) override {
    if (abort_on_component_loaded_) {
      Abort();
    }
  }

  void OnConciergeStarted(CrostiniResult result) override {
    if (abort_on_concierge_started_) {
      Abort();
    }
  }

  void OnDiskImageCreated(CrostiniResult result,
                          vm_tools::concierge::DiskImageStatus status,
                          int64_t disk_size_available) override {
    if (abort_on_disk_image_created_) {
      Abort();
    }
  }

  void OnVmStarted(CrostiniResult result) override {
    if (abort_on_vm_started_) {
      Abort();
    }
  }

  void OnContainerDownloading(int32_t download_percent) override {}

  void OnContainerCreated(CrostiniResult result) override {
    if (abort_on_container_created_) {
      Abort();
    }
  }

  void OnContainerStarted(CrostiniResult result) override {
    if (abort_on_container_started_) {
      Abort();
    }
  }

  void OnContainerSetup(CrostiniResult result) override {
    if (abort_on_container_setup_) {
      Abort();
    }
  }

  void OnSshKeysFetched(CrostiniResult result) override {
    if (abort_on_ssh_keys_fetched_) {
      Abort();
    }
  }

 protected:
  void Abort() {
    crostini_manager()->AbortRestartCrostini(restart_id_,
                                             base::DoNothing::Once());
    run_loop()->Quit();
  }

  void SshfsMount(const std::string& source_path,
                  const std::string& source_format,
                  const std::string& mount_label,
                  const std::vector<std::string>& mount_options,
                  chromeos::MountType type,
                  chromeos::MountAccessMode access_mode) {
    disk_mount_manager_mock_->NotifyMountEvent(
        chromeos::disks::DiskMountManager::MountEvent::MOUNTING,
        chromeos::MountError::MOUNT_ERROR_NONE,
        chromeos::disks::DiskMountManager::MountPointInfo(
            source_path, "/media/fuse/" + mount_label,
            chromeos::MountType::MOUNT_TYPE_NETWORK_STORAGE,
            chromeos::disks::MountCondition::MOUNT_CONDITION_NONE));
  }

  CrostiniManager::RestartId restart_id_ =
      CrostiniManager::kUninitializedRestartId;
  const CrostiniManager::RestartId uninitialized_id_ =
      CrostiniManager::kUninitializedRestartId;
  bool abort_on_component_loaded_ = false;
  bool abort_on_concierge_started_ = false;
  bool abort_on_disk_image_created_ = false;
  bool abort_on_vm_started_ = false;
  bool abort_on_container_created_ = false;
  bool abort_on_container_started_ = false;
  bool abort_on_container_setup_ = false;
  bool abort_on_ssh_keys_fetched_ = false;
  int restart_crostini_callback_count_ = 0;
  int remove_crostini_callback_count_ = 0;
  chromeos::disks::MockDiskMountManager* disk_mount_manager_mock_;
};

TEST_F(CrostiniManagerRestartTest, RestartSuccess) {
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  // Mount only performed for termina/penguin.
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnComponentLoaded) {
  abort_on_component_loaded_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled));
  EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnConciergeStarted) {
  abort_on_concierge_started_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnDiskImageCreated) {
  abort_on_disk_image_created_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnVmStarted) {
  abort_on_vm_started_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerCreated) {
  abort_on_container_created_ = true;
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerCreatedError) {
  abort_on_container_started_ = true;
  fake_cicerone_client_->set_lxd_container_created_signal_status(
      vm_tools::cicerone::LxdContainerCreatedSignal::UNKNOWN);
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerTest::CreateContainerFailsCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();

  EXPECT_TRUE(create_container_fails_callback_called_);
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerStarted) {
  abort_on_container_started_ = true;
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerSetup) {
  abort_on_container_setup_ = true;
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, OnlyMountTerminaPenguin) {
  // Use names other than termina/penguin.  Will not mount sshfs.
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, MultiRestartAllowed) {
  CrostiniManager::RestartId id1, id2, id3;
  id1 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  id2 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  id3 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(id1));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id2));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id3));

  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_EQ(3, restart_crostini_callback_count_);

  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));
}

TEST_F(CrostiniManagerRestartTest, MountForTerminaPenguin) {
  // DiskMountManager mock.  Verify that correct values are received
  // from concierge and passed to DiskMountManager.
  disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
  chromeos::disks::DiskMountManager::InitializeForTesting(
      disk_mount_manager_mock_);
  disk_mount_manager_mock_->SetupDefaultReplies();
  std::string known_hosts;
  base::Base64Encode("[hostname]:2222 pubkey", &known_hosts);
  std::string identity;
  base::Base64Encode("privkey", &identity);
  std::vector<std::string> mount_options = {
      "UserKnownHostsBase64=" + known_hosts, "IdentityBase64=" + identity,
      "Port=2222"};
  EXPECT_CALL(*disk_mount_manager_mock_,
              MountPath("sshfs://testing_profile@hostname:", "",
                        "crostini_test_termina_penguin", mount_options,
                        chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                        chromeos::MOUNT_ACCESS_MODE_READ_WRITE))
      .WillOnce(Invoke(
          this,
          &CrostiniManagerRestartTest_MountForTerminaPenguin_Test::SshfsMount));

  // Use termina/penguin to perform mount.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_TRUE(crostini_manager()
                  ->GetContainerInfo(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName)
                  ->sshfs_mounted);
  EXPECT_EQ(1, restart_crostini_callback_count_);
  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          "crostini_test_termina_penguin", &path));
  EXPECT_EQ(base::FilePath("/media/fuse/crostini_test_termina_penguin"), path);

  chromeos::disks::DiskMountManager::Shutdown();
}

TEST_F(CrostiniManagerRestartTest, IsContainerRunningFalseIfVmNotStarted) {
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));
  run_loop()->Run();

  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  // Mount only performed for termina/penguin.
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);

  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
  EXPECT_TRUE(crostini_manager()->GetContainerInfo(kVmName, kContainerName));

  // Now call StartTerminaVm again. The default response state is "STARTING",
  // so no container should be considered running.
  const base::FilePath& disk_path = base::FilePath(kVmName);

  base::RunLoop run_loop2;
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmSuccessCallback,
                     base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
  EXPECT_FALSE(crostini_manager()->GetContainerInfo(kVmName, kContainerName));
}

TEST_F(CrostiniManagerRestartTest, RestartThenUninstall) {
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));

  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id_));

  run_loop()->Run();

  // Aborts don't call the restart callback. If that changes, everything that
  // calls RestartCrostini will need to be checked to make sure they handle it
  // in a sensible way.
  EXPECT_EQ(0, restart_crostini_callback_count_);
  EXPECT_EQ(1, remove_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, RestartMultipleThenUninstall) {
  CrostiniManager::RestartId id1, id2, id3;
  id1 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));
  id2 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));
  id3 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(id1));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id2));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id3));

  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));

  run_loop()->Run();

  // Aborts don't call the restart callback. If that changes, everything that
  // calls RestartCrostini will need to be checked to make sure they handle it
  // in a sensible way.
  EXPECT_EQ(0, restart_crostini_callback_count_);
  EXPECT_EQ(1, remove_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, UninstallThenRestart) {
  // Install crostini first so that the uninstaller doesn't terminate before we
  // can call the installer again
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));

  run_loop()->Run();

  base::RunLoop run_loop2;
  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop2.QuitClosure()));

  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));

  EXPECT_EQ(uninitialized_id_, restart_id_);

  run_loop2.Run();

  EXPECT_EQ(2, restart_crostini_callback_count_);
  EXPECT_EQ(1, remove_crostini_callback_count_);
}

TEST_F(CrostiniManagerTest, ExportContainerSuccess) {
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  // Send signals, PACK, DOWNLOAD, DONE.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(vm_tools::cicerone::
                        ExportLxdContainerProgressSignal_Status_EXPORTING_PACK);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_DOWNLOAD);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailInProgress) {
  // 1st call succeeds.
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  // 2nd call fails since 1st call is in progress.
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), base::DoNothing::Once(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED));

  // Send signal to indicate 1st call is done.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailFromSignal) {
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED));

  // Send signal with FAILED.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_FAILED);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailOnVmStop) {
  crostini_manager()->AddRunningVmForTesting(kVmName);
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(
          &CrostiniManagerTest::CrostiniResultCallback, base::Unretained(this),
          run_loop()->QuitClosure(),
          CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED));
  crostini_manager()->StopVm(kVmName, base::DoNothing());
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImortContainerSuccess) {
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  // Send signals, UPLOAD, UNPACK, DONE.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UPLOAD);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailInProgress) {
  // 1st call succeeds.
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  // 2nd call fails since 1st call is in progress.
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), base::DoNothing::Once(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED));

  // Send signal to indicate 1st call is done.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailArchitecture) {
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(
          &CrostiniManagerTest::CrostiniResultCallback, base::Unretained(this),
          run_loop()->QuitClosure(),
          CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE));

  // Send signal with FAILED_ARCHITECTURE.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_FAILED_ARCHITECTURE);
  signal.set_architecture_device("archdev");
  signal.set_architecture_container("archcont");
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailFromSignal) {
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED));

  // Send signal with FAILED.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailOnVmStop) {
  crostini_manager()->AddRunningVmForTesting(kVmName);
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(
          &CrostiniManagerTest::CrostiniResultCallback, base::Unretained(this),
          run_loop()->QuitClosure(),
          CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED));
  crostini_manager()->StopVm(kVmName, base::DoNothing());
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, SearchAppSuccess) {
  vm_tools::cicerone::AppSearchResponse response;
  vm_tools::cicerone::AppSearchResponse::AppSearchResult* app =
      response.add_packages();
  app->set_package_name("fake app");
  app = response.add_packages();
  app->set_package_name("fake app1");
  app = response.add_packages();
  app->set_package_name("fake app2");
  fake_cicerone_client_->set_search_app_response(response);
  std::vector<std::string> expected = {"fake app", "fake app1", "fake app2"};
  crostini_manager()->SearchApp(
      kVmName, kContainerName, "fake ap",
      base::BindOnce(&CrostiniManagerTest::SearchAppCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     expected));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, SearchAppNoResults) {
  vm_tools::cicerone::AppSearchResponse response;
  fake_cicerone_client_->set_search_app_response(response);
  std::vector<std::string> expected = {};
  crostini_manager()->SearchApp(
      kVmName, kContainerName, "fake ap",
      base::BindOnce(&CrostiniManagerTest::SearchAppCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     expected));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, GetLinuxPackageInfoFromAptFailedToGetInfo) {
  const char kFailMessage[] = "Failed to get package info.";
  vm_tools::cicerone::LinuxPackageInfoResponse response;
  response.set_success(false);
  response.set_failure_reason(kFailMessage);
  fake_cicerone_client_->set_linux_package_info_response(response);
  LinuxPackageInfo expected;
  expected.success = false;
  expected.failure_reason = kFailMessage;
  crostini_manager()->GetLinuxPackageInfoFromApt(
      kVmName, kContainerName, "fake app",
      base::BindOnce(&CrostiniManagerTest::GetLinuxPackageInfoFromAptCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     expected));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, GetLinuxPackageInfoFromAptInvalidID) {
  vm_tools::cicerone::LinuxPackageInfoResponse response;
  response.set_success(true);
  response.set_package_id("Bad;;id;");
  fake_cicerone_client_->set_linux_package_info_response(response);
  LinuxPackageInfo expected;
  expected.success = false;
  expected.failure_reason = "Linux package info contained invalid package id.";
  crostini_manager()->GetLinuxPackageInfoFromApt(
      kVmName, kContainerName, "fake app",
      base::BindOnce(&CrostiniManagerTest::GetLinuxPackageInfoFromAptCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     expected));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, GetLinuxPackageInfoFromAptSuccess) {
  vm_tools::cicerone::LinuxPackageInfoResponse response;
  response.set_success(true);
  response.set_package_id("good;1.1;id;");
  response.set_summary("A summary");
  response.set_description("A description");
  fake_cicerone_client_->set_linux_package_info_response(response);
  LinuxPackageInfo expected;
  expected.success = true;
  expected.package_id = "good;1.1;id;";
  expected.name = "good";
  expected.version = "1.1";
  expected.summary = "A summary";
  expected.description = "A description";
  crostini_manager()->GetLinuxPackageInfoFromApt(
      kVmName, kContainerName, "fake app",
      base::BindOnce(&CrostiniManagerTest::GetLinuxPackageInfoFromAptCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     expected));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  crostini_manager()->InstallLinuxPackageFromApt(
      kVmName, kContainerName, kPackageID,
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      kVmName, kContainerName, kPackageID,
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalFailure) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason(
      "Unit tests can't install Linux package from apt!");
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      kVmName, kContainerName, kPackageID,
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalOperationBlocked) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      kVmName, kContainerName, kPackageID,
      base::BindOnce(&CrostiniManagerTest::CrostiniResultCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallerStatusInitiallyFalse) {
  EXPECT_FALSE(crostini_manager()->GetInstallerViewStatus());
}

}  // namespace crostini
