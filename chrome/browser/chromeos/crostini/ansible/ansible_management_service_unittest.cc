// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class AnsibleManagementServiceTest : public testing::Test {
 public:
  AnsibleManagementServiceTest() {
    chromeos::DBusThreadManager::Initialize();
    fake_cicerone_client_ = static_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = CrostiniManager::GetForProfile(profile_.get());
    ansible_management_service_ =
        AnsibleManagementService::GetForProfile(profile_.get());
  }
  ~AnsibleManagementServiceTest() override {
    ansible_management_service_->Shutdown();
    crostini_manager_->Shutdown();
    profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  AnsibleManagementService* ansible_management_service() {
    return ansible_management_service_;
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

  void SendSucceededApplicationSignal() {
    vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal signal;
    signal.set_owner_id(CryptohomeIdForProfile(profile()));
    signal.set_vm_name(kCrostiniDefaultVmName);
    signal.set_container_name(kCrostiniDefaultContainerName);
    signal.set_status(
        vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED);

    fake_cicerone_client_->NotifyApplyAnsiblePlaybookProgress(signal);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  CrostiniManager* crostini_manager_;
  AnsibleManagementService* ansible_management_service_;
  base::MockCallback<base::OnceCallback<void(bool)>>
      ansible_installation_finished_mock_callback_;
  base::MockCallback<base::OnceCallback<void(bool)>>
      ansible_playbook_application_finished_mock_callback_;
  // Owned by chromeos::DBusThreadManager
  chromeos::FakeCiceroneClient* fake_cicerone_client_;

  DISALLOW_COPY_AND_ASSIGN(AnsibleManagementServiceTest);
};

TEST_F(AnsibleManagementServiceTest, InstallAnsibleInDefaultContainerSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  CrostiniManager::GetForProfile(profile())
      ->AddLinuxPackageOperationProgressObserver(ansible_management_service());

  EXPECT_CALL(ansible_installation_finished_mock_callback_, Run(true)).Times(1);

  ansible_management_service()->InstallAnsibleInDefaultContainer(
      ansible_installation_finished_mock_callback_.Get());

  // Actually starts installing Ansible.
  base::RunLoop().RunUntilIdle();

  SendSucceededInstallSignal();
}

TEST_F(AnsibleManagementServiceTest, InstallAnsibleInDefaultContainerFail) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  fake_cicerone_client_->set_install_linux_package_response(response);

  EXPECT_CALL(ansible_installation_finished_mock_callback_, Run(false))
      .Times(1);

  ansible_management_service()->InstallAnsibleInDefaultContainer(
      ansible_installation_finished_mock_callback_.Get());

  // Actually starts installing Ansible.
  base::RunLoop().RunUntilIdle();
}

TEST_F(AnsibleManagementServiceTest,
       ApplyAnsiblePlaybookToDefaultContainerSuccess) {
  vm_tools::cicerone::ApplyAnsiblePlaybookResponse response;
  response.set_status(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);
  fake_cicerone_client_->set_apply_ansible_playbook_response(response);

  EXPECT_CALL(ansible_playbook_application_finished_mock_callback_, Run(true))
      .Times(1);

  ansible_management_service()->ApplyAnsiblePlaybookToDefaultContainer(
      /*playbook=*/"",
      ansible_playbook_application_finished_mock_callback_.Get());

  // Actually starts applying Ansible playbook.
  base::RunLoop().RunUntilIdle();

  SendSucceededApplicationSignal();
}

TEST_F(AnsibleManagementServiceTest,
       ApplyAnsiblePlaybookToDefaultContainerFail) {
  vm_tools::cicerone::ApplyAnsiblePlaybookResponse response;
  response.set_status(vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED);
  fake_cicerone_client_->set_apply_ansible_playbook_response(response);

  EXPECT_CALL(ansible_playbook_application_finished_mock_callback_, Run(false))
      .Times(1);

  ansible_management_service()->ApplyAnsiblePlaybookToDefaultContainer(
      /*playbook=*/"",
      ansible_playbook_application_finished_mock_callback_.Get());

  // Actually starts applying Ansible playbook.
  base::RunLoop().RunUntilIdle();
}

}  // namespace crostini
