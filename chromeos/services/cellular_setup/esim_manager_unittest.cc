// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_manager.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {
namespace cellular_setup {

namespace {
const char* kTestEuiccPath = "/org/chromium/Hermes/Euicc/0";
const char* kTestEid = "12345678901234567890123456789012";

void CopyESimProfileList(
    std::vector<mojom::ESimProfilePtr>* dest,
    base::OnceClosure quit_closure,
    base::Optional<std::vector<mojom::ESimProfilePtr>> esim_profiles) {
  if (esim_profiles) {
    for (auto& euicc : *esim_profiles)
      dest->push_back(std::move(euicc));
  }
  std::move(quit_closure).Run();
}

void CopyInstallResult(mojom::ESimManager::ProfileInstallResult* result_dest,
                       mojom::ESimProfilePtr* esim_profile_dest,
                       base::OnceClosure quit_closure,
                       mojom::ESimManager::ProfileInstallResult result,
                       mojom::ESimProfilePtr esim_profile) {
  *result_dest = result;
  *esim_profile_dest = std::move(esim_profile);
  std::move(quit_closure).Run();
}

void CopyOperationResult(mojom::ESimManager::ESimOperationResult* result_dest,
                         base::OnceClosure quit_closure,
                         mojom::ESimManager::ESimOperationResult result) {
  *result_dest = result;
  std::move(quit_closure).Run();
}

}  // namespace

// Fake observer for testing ESimManager.
class ESimManagerTestObserver : public mojom::ESimManagerObserver {
 public:
  ESimManagerTestObserver() = default;
  ESimManagerTestObserver(const ESimManagerTestObserver&) = delete;
  ESimManagerTestObserver& operator=(const ESimManagerTestObserver&) = delete;
  ~ESimManagerTestObserver() override = default;

  // mojom::ESimManagerObserver:
  void OnAvailableEuiccListChanged() override {
    available_euicc_list_change_count_++;
  }
  void OnProfileListChanged(const std::string& eid) override {
    profile_list_change_calls_.push_back(eid);
  }
  void OnEuiccChanged(mojom::EuiccPtr euicc) override {
    euicc_change_calls_.push_back(std::move(euicc));
  }
  void OnProfileChanged(mojom::ESimProfilePtr esim_profile) override {
    profile_change_calls_.push_back(std::move(esim_profile));
  }

  mojo::PendingRemote<mojom::ESimManagerObserver> GenerateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Reset() {
    available_euicc_list_change_count_ = 0;
    profile_list_change_calls_.clear();
    euicc_change_calls_.clear();
    profile_change_calls_.clear();
  }

  int available_euicc_list_change_count() {
    return available_euicc_list_change_count_;
  }

  const std::vector<std::string>& profile_list_change_calls() {
    return profile_list_change_calls_;
  }

  const std::vector<mojom::EuiccPtr>& euicc_change_calls() {
    return euicc_change_calls_;
  }

  const std::vector<mojom::ESimProfilePtr>& profile_change_calls() {
    return profile_change_calls_;
  }

 private:
  int available_euicc_list_change_count_ = 0;
  std::vector<std::string> profile_list_change_calls_;
  std::vector<mojom::EuiccPtr> euicc_change_calls_;
  std::vector<mojom::ESimProfilePtr> profile_change_calls_;
  mojo::Receiver<mojom::ESimManagerObserver> receiver_{this};
};

class ESimManagerTest : public testing::Test {
 public:
  using InstallResultPair = std::pair<mojom::ESimManager::ProfileInstallResult,
                                      mojom::ESimProfilePtr>;

  ESimManagerTest() {
    if (!ShillManagerClient::Get())
      shill_clients::InitializeFakes();
    if (!HermesManagerClient::Get())
      hermes_clients::InitializeFakes();
  }
  ESimManagerTest(const ESimManagerTest&) = delete;
  ESimManagerTest& operator=(const ESimManagerTest&) = delete;
  ~ESimManagerTest() override = default;

  void SetUp() override {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
        base::TimeDelta::FromSeconds(0));
    esim_manager_ = std::make_unique<ESimManager>();
    observer_ = std::make_unique<ESimManagerTestObserver>();
    esim_manager_->AddObserver(observer_->GenerateRemote());
  }

  void TearDown() override {
    esim_manager_.reset();
    observer_.reset();
    HermesEuiccClient::Get()->GetTestInterface()->ResetPendingEventsRequested();
  }

  void SetupEuicc() {
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid, true);
    base::RunLoop().RunUntilIdle();
  }

  std::vector<mojom::EuiccPtr> GetAvailableEuiccs() {
    std::vector<mojom::EuiccPtr> result;
    base::RunLoop run_loop;
    esim_manager_->GetAvailableEuiccs(base::BindOnce(
        [](std::vector<mojom::EuiccPtr>* result, base::OnceClosure quit_closure,
           std::vector<mojom::EuiccPtr> available_euiccs) {
          for (auto& euicc : available_euiccs)
            result->push_back(std::move(euicc));
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  std::vector<mojom::ESimProfilePtr> GetProfiles(const std::string& eid) {
    std::vector<mojom::ESimProfilePtr> result;
    base::RunLoop run_loop;
    esim_manager_->GetProfiles(
        eid,
        base::BindOnce(&CopyESimProfileList, &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  mojom::ESimManager::ESimOperationResult RequestPendingProfiles(
      const std::string& eid) {
    mojom::ESimManager::ESimOperationResult result;
    base::RunLoop run_loop;
    esim_manager_->RequestPendingProfiles(
        eid, base::BindOnce(
                 [](mojom::ESimManager::ESimOperationResult* result,
                    base::OnceClosure quit_closure,
                    mojom::ESimManager::ESimOperationResult status) {
                   *result = status;
                   std::move(quit_closure).Run();
                 },
                 &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  InstallResultPair InstallProfileFromActivationCode(
      const std::string& eid,
      const std::string& activation_code,
      const std::string& confirmation_code) {
    base::RunLoop run_loop;
    mojom::ESimManager::ProfileInstallResult result;
    mojom::ESimProfilePtr esim_profile;
    esim_manager_->InstallProfileFromActivationCode(
        eid, activation_code, confirmation_code,
        base::BindOnce(&CopyInstallResult, &result, &esim_profile,
                       run_loop.QuitClosure()));
    run_loop.Run();
    return std::make_pair(result, std::move(esim_profile));
  }

  InstallResultPair InstallProfile(const std::string& iccid,
                                   const std::string& confirmation_code) {
    base::RunLoop run_loop;
    mojom::ESimManager::ProfileInstallResult result;
    mojom::ESimProfilePtr esim_profile;
    esim_manager_->InstallProfile(
        iccid, confirmation_code,
        base::BindOnce(&CopyInstallResult, &result, &esim_profile,
                       run_loop.QuitClosure()));
    run_loop.Run();
    return std::make_pair(result, std::move(esim_profile));
  }

  mojom::ESimManager::ESimOperationResult UninstallProfile(
      const std::string& iccid) {
    base::RunLoop run_loop;
    mojom::ESimManager::ESimOperationResult result;
    esim_manager_->UninstallProfile(
        iccid,
        base::BindOnce(&CopyOperationResult, &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  mojom::ESimManager::ESimOperationResult EnableProfile(
      const std::string& iccid) {
    base::RunLoop run_loop;
    mojom::ESimManager::ESimOperationResult result;
    esim_manager_->EnableProfile(
        iccid,
        base::BindOnce(&CopyOperationResult, &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  mojom::ESimManager::ESimOperationResult DisableProfile(
      const std::string& iccid) {
    base::RunLoop run_loop;
    mojom::ESimManager::ESimOperationResult result;
    esim_manager_->DisableProfile(
        iccid,
        base::BindOnce(&CopyOperationResult, &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  mojom::ESimManager::ESimOperationResult SetProfileNickname(
      const std::string& iccid,
      const base::string16& nickname) {
    base::RunLoop run_loop;
    mojom::ESimManager::ESimOperationResult result;
    esim_manager_->SetProfileNickname(
        iccid, nickname,
        base::BindOnce(&CopyOperationResult, &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  ESimManager* esim_manager() { return esim_manager_.get(); }
  ESimManagerTestObserver* observer() { return observer_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ESimManager> esim_manager_;
  std::unique_ptr<ESimManagerTestObserver> observer_;
};

TEST_F(ESimManagerTest, GetAvailableEuiccs) {
  ASSERT_EQ(0u, GetAvailableEuiccs().size());
  // Verify that Euicc List change is notified to observer when a
  // a new Euicc is setup.
  SetupEuicc();
  EXPECT_EQ(1, observer()->available_euicc_list_change_count());
  // Verify that GetAvailableEuiccs call returns list of euiccs.
  std::vector<mojom::EuiccPtr> available_euiccs = GetAvailableEuiccs();
  ASSERT_EQ(1u, available_euiccs.size());
  EXPECT_EQ(kTestEid, available_euiccs.front()->eid);
}

TEST_F(ESimManagerTest, GetProfiles) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* active_profile_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);
  HermesProfileClient::Properties* pending_profile_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  // Verify the profile list change is notified to observer.
  ASSERT_EQ(2u, observer()->profile_list_change_calls().size());
  EXPECT_EQ(kTestEid, observer()->profile_list_change_calls().at(0));
  EXPECT_EQ(kTestEid, observer()->profile_list_change_calls().at(1));
  // Verify that the added profile is returned in installed list.
  std::vector<mojom::ESimProfilePtr> profile_list = GetProfiles(kTestEid);
  ASSERT_EQ(2u, profile_list.size());
  EXPECT_EQ(active_profile_properties->iccid().value(),
            profile_list.at(0)->iccid);
  EXPECT_EQ(pending_profile_properties->iccid().value(),
            profile_list.at(1)->iccid);
}

TEST_F(ESimManagerTest, RequestPendingProfiles) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  // Verify that pending profile request errors are return properly.
  euicc_test->QueueHermesErrorStatus(HermesResponseStatus::kErrorNoResponse);
  mojom::ESimManager::ESimOperationResult result =
      RequestPendingProfiles(kTestEid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_list_change_calls().size());

  // Verify that successful request notifies observers and returns correct
  // status code.
  result = RequestPendingProfiles(kTestEid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kSuccess, result);
  EXPECT_EQ(kTestEid, observer()->profile_list_change_calls().front());
}

TEST_F(ESimManagerTest, InstallProfileFromActivationCode) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  // Verify that install errors return error code properly.
  euicc_test->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorInvalidActivationCode);
  InstallResultPair result_pair =
      InstallProfileFromActivationCode(kTestEid, "", "");
  EXPECT_EQ(
      mojom::ESimManager::ProfileInstallResult::kErrorInvalidActivationCode,
      result_pair.first);
  EXPECT_EQ(nullptr, result_pair.second.get());

  // Verify that installing a profile returns proper status code
  // and profile object.
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  result_pair = InstallProfileFromActivationCode(
      kTestEid, properties->activation_code().value(), "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ProfileInstallResult::kSuccess,
            result_pair.first);
  ASSERT_NE(nullptr, result_pair.second.get());
  EXPECT_EQ(properties->iccid().value(), result_pair.second->iccid);
  EXPECT_EQ(3u, observer()->profile_list_change_calls().size());
}

TEST_F(ESimManagerTest, InstallProfile) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(profile_path);

  // Verify that install errors return error code properly.
  euicc_test->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorNeedConfirmationCode);
  InstallResultPair result_pair =
      InstallProfile(properties->iccid().value(), "");
  EXPECT_EQ(
      mojom::ESimManager::ProfileInstallResult::kErrorNeedsConfirmationCode,
      result_pair.first);
  EXPECT_EQ(nullptr, result_pair.second.get());

  // Verify that installing pending profile returns proper results.
  result_pair = InstallProfile(properties->iccid().value(), "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ProfileInstallResult::kSuccess,
            result_pair.first);
  ASSERT_NE(nullptr, result_pair.second.get());
  ASSERT_EQ(properties->iccid().value(), result_pair.second->iccid);
  EXPECT_EQ(3u, observer()->profile_list_change_calls().size());
}

TEST_F(ESimManagerTest, UninstallProfile) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* active_profile_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);

  // Verify that uninstall error codes are returned properly.
  euicc_test->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorInvalidResponse);
  mojom::ESimManager::ESimOperationResult result =
      UninstallProfile(active_profile_properties->iccid().value());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_list_change_calls().size());

  // Verify that pending profiles cannot be uninstalled
  observer()->Reset();
  result = UninstallProfile(pending_profile_properties->iccid().value());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_list_change_calls().size());

  // Verify that uninstall removes the profile and notifies observers properly.
  observer()->Reset();
  result = UninstallProfile(active_profile_properties->iccid().value());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kSuccess, result);
  ASSERT_EQ(1u, observer()->profile_list_change_calls().size());
  EXPECT_EQ(kTestEid, observer()->profile_list_change_calls().front());
}

TEST_F(ESimManagerTest, EnableProfile) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath inactive_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kInactive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* inactive_profile_properties =
      HermesProfileClient::Get()->GetProperties(inactive_profile_path);

  // Verify that pending profiles cannot be enabled.
  mojom::ESimManager::ESimOperationResult result =
      EnableProfile(pending_profile_properties->iccid().value());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_change_calls().size());

  // Verify that enabling profile returns result properly.
  result = EnableProfile(inactive_profile_properties->iccid().value());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kSuccess, result);
  EXPECT_EQ(inactive_profile_properties->iccid().value(),
            observer()->profile_change_calls().front()->iccid);
  EXPECT_EQ(mojom::ProfileState::kActive,
            observer()->profile_change_calls().front()->state);
}

TEST_F(ESimManagerTest, DisableProfile) {
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* active_profile_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);

  // Verify that pending profiles cannot be disabled.
  mojom::ESimManager::ESimOperationResult result =
      DisableProfile(pending_profile_properties->iccid().value());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_change_calls().size());

  // Verify that disabling profile returns result properly.
  result = DisableProfile(active_profile_properties->iccid().value());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kSuccess, result);
  EXPECT_EQ(active_profile_properties->iccid().value(),
            observer()->profile_change_calls().front()->iccid);
  EXPECT_EQ(mojom::ProfileState::kInactive,
            observer()->profile_change_calls().front()->state);
}

TEST_F(ESimManagerTest, SetProfileNickName) {
  const base::string16 test_nickname = base::UTF8ToUTF16("Test nickname");
  SetupEuicc();
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* active_profile_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);

  // Verify that pending profiles cannot be modified.
  mojom::ESimManager::ESimOperationResult result = SetProfileNickname(
      pending_profile_properties->iccid().value(), test_nickname);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_change_calls().size());

  // Verify that nickname can be set on active profiles.
  result = SetProfileNickname(active_profile_properties->iccid().value(),
                              test_nickname);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimManager::ESimOperationResult::kSuccess, result);
  EXPECT_EQ(active_profile_properties->iccid().value(),
            observer()->profile_change_calls().front()->iccid);
  EXPECT_EQ(test_nickname,
            observer()->profile_change_calls().front()->nickname);
}

}  // namespace cellular_setup
}  // namespace chromeos
