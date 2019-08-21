// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_ansible_management_service.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class CrostiniAnsibleManagementServiceTest : public testing::Test {
 public:
  CrostiniAnsibleManagementServiceTest() {
    chromeos::DBusThreadManager::Initialize();
    fake_cicerone_client_ = static_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
    profile_ = std::make_unique<TestingProfile>();
    crostini_ansible_management_service_ =
        std::make_unique<CrostiniAnsibleManagementService>(profile_.get());
  }
  ~CrostiniAnsibleManagementServiceTest() override {
    crostini_ansible_management_service_.reset();
    profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  CrostiniAnsibleManagementService* crostini_ansible_management_service() {
    return crostini_ansible_management_service_.get();
  }

  void SendSucceededInstallSignal() {
    vm_tools::cicerone::InstallLinuxPackageProgressSignal signal;
    signal.set_owner_id(CryptohomeIdForProfile(profile()));
    signal.set_vm_name(kCrostiniDefaultVmName);
    signal.set_container_name(kCrostiniDefaultContainerName);
    signal.set_status(
        vm_tools::cicerone::InstallLinuxPackageProgressSignal::SUCCEEDED);

    fake_cicerone_client_->InstallLinuxPackageProgress(signal);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniAnsibleManagementService>
      crostini_ansible_management_service_;
  base::MockCallback<base::OnceCallback<void(bool)>>
      ansible_installation_finished_mock_callback_;
  // Owned by chromeos::DBusThreadManager
  chromeos::FakeCiceroneClient* fake_cicerone_client_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAnsibleManagementServiceTest);
};

TEST_F(CrostiniAnsibleManagementServiceTest,
       InstallAnsibleInDefaultContainerSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  CrostiniManager::GetForProfile(profile())
      ->AddLinuxPackageOperationProgressObserver(
          crostini_ansible_management_service());

  EXPECT_CALL(ansible_installation_finished_mock_callback_, Run(true)).Times(1);

  crostini_ansible_management_service()->InstallAnsibleInDefaultContainer(
      ansible_installation_finished_mock_callback_.Get());

  // Actually starts installing Ansible.
  base::RunLoop().RunUntilIdle();

  SendSucceededInstallSignal();
}

TEST_F(CrostiniAnsibleManagementServiceTest,
       InstallAnsibleInDefaultContainerFail) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  fake_cicerone_client_->set_install_linux_package_response(response);

  EXPECT_CALL(ansible_installation_finished_mock_callback_, Run(false))
      .Times(1);

  crostini_ansible_management_service()->InstallAnsibleInDefaultContainer(
      ansible_installation_finished_mock_callback_.Get());

  // Actually starts installing Ansible.
  base::RunLoop().RunUntilIdle();
}

}  // namespace crostini
