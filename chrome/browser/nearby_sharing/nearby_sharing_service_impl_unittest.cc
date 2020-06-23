// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using testing::NiceMock;
using testing::Return;

class FakeFastInitiationManager : public FastInitiationManager {
 public:
  explicit FakeFastInitiationManager(
      scoped_refptr<device::BluetoothAdapter> adapter,
      bool should_succeed_on_start,
      base::OnceCallback<void()> on_stop_advertising_callback,
      base::OnceCallback<void()> on_destroy_callback)
      : FastInitiationManager(adapter),
        should_succeed_on_start_(should_succeed_on_start),
        on_stop_advertising_callback_(std::move(on_stop_advertising_callback)),
        on_destroy_callback_(std::move(on_destroy_callback)) {}

  ~FakeFastInitiationManager() override {
    std::move(on_destroy_callback_).Run();
  }

  void StartAdvertising(base::OnceCallback<void()> callback,
                        base::OnceCallback<void()> error_callback) override {
    ++start_advertising_call_count_;
    if (should_succeed_on_start_)
      std::move(callback).Run();
    else
      std::move(error_callback).Run();
  }

  void StopAdvertising(base::OnceCallback<void()> callback) override {
    std::move(on_stop_advertising_callback_).Run();
    std::move(callback).Run();
  }

  size_t start_advertising_call_count() {
    return start_advertising_call_count_;
  }

 private:
  bool should_succeed_on_start_;
  size_t start_advertising_call_count_ = 0u;
  base::OnceCallback<void()> on_stop_advertising_callback_;
  base::OnceCallback<void()> on_destroy_callback_;
};

class FakeFastInitiationManagerFactory : public FastInitiationManager::Factory {
 public:
  explicit FakeFastInitiationManagerFactory(bool should_succeed_on_start)
      : should_succeed_on_start_(should_succeed_on_start) {}

  std::unique_ptr<FastInitiationManager> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter) override {
    auto fake_fast_initiation_manager = std::make_unique<
        FakeFastInitiationManager>(
        adapter, should_succeed_on_start_,
        base::BindOnce(&FakeFastInitiationManagerFactory::OnStopAdvertising,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(
            &FakeFastInitiationManagerFactory::OnFastInitiationManagerDestroyed,
            weak_ptr_factory_.GetWeakPtr()));
    last_fake_fast_initiation_manager_ = fake_fast_initiation_manager.get();
    return std::move(fake_fast_initiation_manager);
  }

  void OnStopAdvertising() { stop_advertising_called_ = true; }

  void OnFastInitiationManagerDestroyed() {
    fast_initiation_manager_destroyed_ = true;
    last_fake_fast_initiation_manager_ = nullptr;
  }

  size_t StartAdvertisingCount() {
    return last_fake_fast_initiation_manager_->start_advertising_call_count();
  }

  bool StopAdvertisingCalledAndManagerDestroyed() {
    return stop_advertising_called_ && fast_initiation_manager_destroyed_;
  }

 private:
  FakeFastInitiationManager* last_fake_fast_initiation_manager_ = nullptr;
  bool should_succeed_on_start_ = false;
  bool stop_advertising_called_ = false;
  bool fast_initiation_manager_destroyed_ = false;
  base::WeakPtrFactory<FakeFastInitiationManagerFactory> weak_ptr_factory_{
      this};
};

namespace {

class NearbySharingServiceImplTest : public testing::Test {
 public:
  NearbySharingServiceImplTest() {
    RegisterNearbySharingPrefs(prefs_.registry());
  }

  ~NearbySharingServiceImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(
            Invoke(this, &NearbySharingServiceImplTest::IsBluetoothPresent));
    ON_CALL(*mock_bluetooth_adapter_, IsPowered())
        .WillByDefault(
            Invoke(this, &NearbySharingServiceImplTest::IsBluetoothPowered));
    ON_CALL(*mock_bluetooth_adapter_, AddObserver(_))
        .WillByDefault(
            Invoke(this, &NearbySharingServiceImplTest::AddAdapterObserver));
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);

    service_ = CreateService("name");
    SetFakeFastInitiationManagerFactory(/*should_succeed_on_start=*/true);
  }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  std::unique_ptr<NearbySharingServiceImpl> CreateService(
      const std::string& profile_name) {
    Profile* profile = profile_manager_.CreateTestingProfile(profile_name);
    fake_nearby_connections_manager_ = new FakeNearbyConnectionsManager();
    auto service = std::make_unique<NearbySharingServiceImpl>(
        &prefs_, profile, base::WrapUnique(fake_nearby_connections_manager_));

    // Allow the posted task to fetch the BluetoothAdapter to finish.
    base::RunLoop().RunUntilIdle();

    return service;
  }

  void SetFakeFastInitiationManagerFactory(bool should_succeed_on_start) {
    fast_initiation_manager_factory_ =
        std::make_unique<FakeFastInitiationManagerFactory>(
            should_succeed_on_start);
    FastInitiationManager::Factory::SetFactoryForTesting(
        fast_initiation_manager_factory_.get());
  }

  NearbySharingService::StatusCodes RegisterSendSurfaceAndWait() {
    base::RunLoop run_loop;
    NearbySharingService::StatusCodes result;
    service_->RegisterSendSurface(
        /*transferCallback=*/nullptr, /*discoveryCallback=*/nullptr,
        base::BindOnce(
            [](base::OnceClosure quit_closure,
               NearbySharingService::StatusCodes* result,
               NearbySharingService::StatusCodes code) {
              *result = code;
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  NearbySharingService::StatusCodes UnregisterSendSurfaceAndWait() {
    base::RunLoop run_loop;
    NearbySharingService::StatusCodes result;
    service_->UnregisterSendSurface(
        /*transferCallback=*/nullptr, /*discoveryCallback=*/nullptr,
        base::BindOnce(
            [](base::OnceClosure quit_closure,
               NearbySharingService::StatusCodes* result,
               NearbySharingService::StatusCodes code) {
              *result = code;
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  bool IsBluetoothPresent() { return is_bluetooth_present_; }
  bool IsBluetoothPowered() { return is_bluetooth_powered_; }

  void AddAdapterObserver(device::BluetoothAdapter::Observer* observer) {
    DCHECK(!adapter_observer_);
    adapter_observer_ = observer;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  sync_preferences::TestingPrefServiceSyncable prefs_;
  FakeNearbyConnectionsManager* fake_nearby_connections_manager_ = nullptr;
  std::unique_ptr<NearbySharingServiceImpl> service_;
  std::unique_ptr<FakeFastInitiationManagerFactory>
      fast_initiation_manager_factory_;
  bool is_bluetooth_present_ = true;
  bool is_bluetooth_powered_ = true;
  device::BluetoothAdapter::Observer* adapter_observer_ = nullptr;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
};

}  // namespace

TEST_F(NearbySharingServiceImplTest, AddsNearbyProcessObserver) {
  NearbyProcessManager& manager = NearbyProcessManager::GetInstance();
  EXPECT_TRUE(manager.observers_.HasObserver(service_.get()));
}

TEST_F(NearbySharingServiceImplTest, RemovesNearbyProcessObserver) {
  service_.reset();
  NearbyProcessManager& manager = NearbyProcessManager::GetInstance();
  EXPECT_FALSE(manager.observers_.might_have_observers());
}

TEST_F(NearbySharingServiceImplTest, DisableNearbyShutdownConnections) {
  prefs_.SetBoolean(prefs::kNearbySharingEnabledPrefName, false);
  EXPECT_TRUE(fake_nearby_connections_manager_->IsShutdown());
}

TEST_F(NearbySharingServiceImplTest, StartFastInitiationAdvertising) {
  EXPECT_EQ(NearbySharingService::StatusCodes::kOk,
            RegisterSendSurfaceAndWait());
  EXPECT_EQ(1u, fast_initiation_manager_factory_->StartAdvertisingCount());

  // Call RegisterSendSurface() a second time and make sure StartAdvertising is
  // not called again.
  EXPECT_EQ(NearbySharingService::StatusCodes::kOk,
            RegisterSendSurfaceAndWait());
  EXPECT_EQ(1u, fast_initiation_manager_factory_->StartAdvertisingCount());
}

TEST_F(NearbySharingServiceImplTest, StartFastInitiationAdvertisingError) {
  SetFakeFastInitiationManagerFactory(/*should_succeed_on_start=*/false);
  EXPECT_EQ(NearbySharingService::StatusCodes::kError,
            RegisterSendSurfaceAndWait());
}

TEST_F(NearbySharingServiceImplTest,
       StartFastInitiationAdvertising_BluetoothNotPresent) {
  is_bluetooth_present_ = false;
  EXPECT_EQ(NearbySharingService::StatusCodes::kError,
            RegisterSendSurfaceAndWait());
}

TEST_F(NearbySharingServiceImplTest,
       StartFastInitiationAdvertising_BluetoothNotPowered) {
  is_bluetooth_powered_ = false;
  EXPECT_EQ(NearbySharingService::StatusCodes::kError,
            RegisterSendSurfaceAndWait());
}

TEST_F(NearbySharingServiceImplTest, StopFastInitiationAdvertising) {
  EXPECT_EQ(NearbySharingService::StatusCodes::kOk,
            RegisterSendSurfaceAndWait());
  EXPECT_EQ(1u, fast_initiation_manager_factory_->StartAdvertisingCount());
  EXPECT_EQ(NearbySharingService::StatusCodes::kOk,
            UnregisterSendSurfaceAndWait());
  EXPECT_TRUE(fast_initiation_manager_factory_
                  ->StopAdvertisingCalledAndManagerDestroyed());
}

TEST_F(NearbySharingServiceImplTest,
       StopFastInitiationAdvertising_BluetoothBecomesNotPresent) {
  EXPECT_EQ(NearbySharingService::StatusCodes::kOk,
            RegisterSendSurfaceAndWait());
  adapter_observer_->AdapterPresentChanged(mock_bluetooth_adapter_.get(),
                                           false);
  EXPECT_TRUE(fast_initiation_manager_factory_
                  ->StopAdvertisingCalledAndManagerDestroyed());
}

TEST_F(NearbySharingServiceImplTest,
       StopFastInitiationAdvertising_BluetoothBecomesNotPowered) {
  EXPECT_EQ(NearbySharingService::StatusCodes::kOk,
            RegisterSendSurfaceAndWait());
  adapter_observer_->AdapterPoweredChanged(mock_bluetooth_adapter_.get(),
                                           false);
  EXPECT_TRUE(fast_initiation_manager_factory_
                  ->StopAdvertisingCalledAndManagerDestroyed());
}
