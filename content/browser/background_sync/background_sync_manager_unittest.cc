// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/mock_background_sync_controller.h"
#include "content/test/test_background_sync_manager.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Return;

const char kScope1[] = "https://example.com/a";
const char kScope2[] = "https://example.com/b";
const char kScript1[] = "https://example.com/a/script.js";
const char kScript2[] = "https://example.com/b/script.js";

void RegisterServiceWorkerCallback(bool* called,
                                   int64_t* store_registration_id,
                                   blink::ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64_t registration_id) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

void FindServiceWorkerRegistrationCallback(
    scoped_refptr<ServiceWorkerRegistration>* out_registration,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);
  *out_registration = std::move(registration);
}

void UnregisterServiceWorkerCallback(bool* called,
                                     blink::ServiceWorkerStatusCode code) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, code);
  *called = true;
}

blink::mojom::BackgroundSyncType GetBackgroundSyncType(
    const blink::mojom::SyncRegistrationOptions& options) {
  return options.min_interval == -1
             ? blink::mojom::BackgroundSyncType::ONE_SHOT
             : blink::mojom::BackgroundSyncType::PERIODIC;
}

}  // namespace

class BackgroundSyncManagerTest
    : public testing::Test,
      public DevToolsBackgroundServicesContextImpl::EventObserver {
 public:
  BackgroundSyncManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
    sync_options_1_.tag = "foo";
    sync_options_2_.tag = "bar";
  }

  void SetUp() override {
    // Don't let the tests be confused by the real-world device connectivity
    background_sync_test_util::SetIgnoreNetworkChanges(true);

    // TODO(jkarlin): Create a new object with all of the necessary SW calls
    // so that we can inject test versions instead of bringing up all of this
    // extra SW stuff.
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    std::unique_ptr<MockPermissionManager> mock_permission_manager(
        new testing::NiceMock<MockPermissionManager>());
    ON_CALL(*mock_permission_manager,
            GetPermissionStatus(PermissionType::BACKGROUND_SYNC, _, _))
        .WillByDefault(Return(blink::mojom::PermissionStatus::GRANTED));
    ON_CALL(*mock_permission_manager,
            GetPermissionStatus(PermissionType::PERIODIC_BACKGROUND_SYNC, _, _))
        .WillByDefault(Return(blink::mojom::PermissionStatus::GRANTED));
    ON_CALL(*mock_permission_manager,
            GetPermissionStatus(PermissionType::NOTIFICATIONS, _, _))
        .WillByDefault(Return(blink::mojom::PermissionStatus::DENIED));
    helper_->browser_context()->SetPermissionControllerDelegate(
        std::move(mock_permission_manager));

    // Create a StoragePartition with the correct BrowserContext so that the
    // BackgroundSyncManager can find the BrowserContext through it.
    storage_partition_impl_ = StoragePartitionImpl::Create(
        helper_->browser_context(), /* in_memory= */ true, base::FilePath(),
        /* partition_domain= */ "");
    helper_->context_wrapper()->set_storage_partition(
        storage_partition_impl_.get());

    SetMaxSyncAttemptsAndRestartManager(1);

    // Wait for storage to finish initializing before registering service
    // workers.
    base::RunLoop().RunUntilIdle();
    RegisterServiceWorkers();

    storage_partition_impl_->GetDevToolsBackgroundServicesContext()
        ->AddObserver(this);
  }

  void TearDown() override {
    storage_partition_impl_->GetDevToolsBackgroundServicesContext()
        ->RemoveObserver(this);
    // Restore the network observer functionality for subsequent tests.
    background_sync_test_util::SetIgnoreNetworkChanges(false);
  }

  void RegisterServiceWorkers() {
    bool called_1 = false;
    bool called_2 = false;
    blink::mojom::ServiceWorkerRegistrationOptions options1;
    options1.scope = GURL(kScope1);
    blink::mojom::ServiceWorkerRegistrationOptions options2;
    options2.scope = GURL(kScope2);
    helper_->context()->RegisterServiceWorker(
        GURL(kScript1), options1,
        base::BindOnce(&RegisterServiceWorkerCallback, &called_1,
                       &sw_registration_id_1_));

    helper_->context()->RegisterServiceWorker(
        GURL(kScript2), options2,
        base::BindOnce(&RegisterServiceWorkerCallback, &called_2,
                       &sw_registration_id_2_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called_1);
    EXPECT_TRUE(called_2);

    // Hang onto the registrations as they need to be "live" when
    // calling BackgroundSyncManager::Register.
    helper_->context_wrapper()->FindReadyRegistrationForId(
        sw_registration_id_1_, GURL(kScope1).GetOrigin(),
        base::BindOnce(FindServiceWorkerRegistrationCallback,
                       &sw_registration_1_));

    helper_->context_wrapper()->FindReadyRegistrationForId(
        sw_registration_id_2_, GURL(kScope1).GetOrigin(),
        base::BindOnce(FindServiceWorkerRegistrationCallback,
                       &sw_registration_2_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(sw_registration_1_);
    EXPECT_TRUE(sw_registration_2_);
  }

  void SetNetwork(network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
    if (test_background_sync_manager_) {
      BackgroundSyncNetworkObserver* network_observer =
          test_background_sync_manager_->GetNetworkObserverForTesting();
      network_observer->NotifyManagerIfConnectionChangedForTesting(
          connection_type);
      base::RunLoop().RunUntilIdle();
    }
  }

  void StatusAndOneShotSyncRegistrationCallback(
      bool* was_called,
      BackgroundSyncStatus status,
      std::unique_ptr<BackgroundSyncRegistration> registration) {
    *was_called = true;
    one_shot_sync_callback_status_ = status;
    callback_one_shot_sync_registration_ = std::move(registration);
  }

  void StatusAndPeriodicSyncRegistrationCallback(
      bool* was_called,
      BackgroundSyncStatus status,
      std::unique_ptr<BackgroundSyncRegistration> registration) {
    *was_called = true;
    periodic_sync_callback_status_ = status;
    callback_periodic_sync_registration_ = std::move(registration);
  }

  void StatusAndOneShotSyncRegistrationsCallback(
      bool* was_called,
      BackgroundSyncStatus status,
      std::vector<std::unique_ptr<BackgroundSyncRegistration>> registrations) {
    *was_called = true;
    one_shot_sync_callback_status_ = status;
    callback_one_shot_sync_registrations_ = std::move(registrations);
  }

  void StatusAndPeriodicSyncRegistrationsCallback(
      bool* was_called,
      BackgroundSyncStatus status,
      std::vector<std::unique_ptr<BackgroundSyncRegistration>> registrations) {
    *was_called = true;
    periodic_sync_callback_status_ = status;
    callback_periodic_sync_registrations_ = std::move(registrations);
  }

  void StatusCallback(bool* was_called, BackgroundSyncStatus status) {
    *was_called = true;
    callback_status_ = status;
  }

  base::TimeDelta GetSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type) {
    return test_background_sync_manager_->GetSoonestWakeupDelta(sync_type);
  }

 protected:
  MOCK_METHOD1(OnEventReceived,
               void(const devtools::proto::BackgroundServiceEvent&));
  MOCK_METHOD2(OnRecordingStateChanged,
               void(bool, devtools::proto::BackgroundService));

  void CreateBackgroundSyncManager() {
    test_background_sync_manager_ = std::make_unique<TestBackgroundSyncManager>(
        helper_->context_wrapper(),
        storage_partition_impl_->GetDevToolsBackgroundServicesContext());

    test_background_sync_manager_->set_clock(&test_clock_);

    // Many tests do not expect the sync event to fire immediately after
    // register (and cleanup up the sync registrations).  Tests can control when
    // the sync event fires by manipulating the network state as needed.
    // NOTE: The setup of the network connection must happen after the
    //       BackgroundSyncManager has been created.
    SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  }

  void InitBackgroundSyncManager() {
    test_background_sync_manager_->DoInit();
    base::RunLoop().RunUntilIdle();
  }

  void SetupBackgroundSyncManager() {
    CreateBackgroundSyncManager();
    InitBackgroundSyncManager();
  }

  void SetupCorruptBackgroundSyncManager() {
    CreateBackgroundSyncManager();
    test_background_sync_manager_->set_corrupt_backend(true);
    InitBackgroundSyncManager();
  }

  void SetupDelayedBackgroundSyncManager() {
    CreateBackgroundSyncManager();
    test_background_sync_manager_->set_delay_backend(true);
    InitBackgroundSyncManager();
  }

  void DeleteBackgroundSyncManager() {
    test_background_sync_manager_ = nullptr;
  }

  bool Register(blink::mojom::SyncRegistrationOptions sync_options) {
    return RegisterWithServiceWorkerId(sw_registration_id_1_,
                                       std::move(sync_options));
  }

  bool Unregister(blink::mojom::SyncRegistrationOptions sync_options) {
    return UnregisterWithServiceWorkerId(sw_registration_id_1_,
                                         std::move(sync_options));
  }

  bool RegisterWithServiceWorkerId(
      int64_t sw_registration_id,
      blink::mojom::SyncRegistrationOptions options) {
    bool was_called = false;
    BackgroundSyncStatus* callback_status;
    if (GetBackgroundSyncType(options) ==
        blink::mojom::BackgroundSyncType::ONE_SHOT) {
      test_background_sync_manager_->Register(
          sw_registration_id, std::move(options),
          base::BindOnce(&BackgroundSyncManagerTest::
                             StatusAndOneShotSyncRegistrationCallback,
                         base::Unretained(this), &was_called));
      callback_status = &one_shot_sync_callback_status_;
    } else {
      test_background_sync_manager_->Register(
          sw_registration_id, std::move(options),
          base::BindOnce(&BackgroundSyncManagerTest::
                             StatusAndPeriodicSyncRegistrationCallback,
                         base::Unretained(this), &was_called));
      callback_status = &periodic_sync_callback_status_;
    }

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    // Mock the client receiving the response and calling
    // DidResolveRegistration.
    if (*callback_status == BACKGROUND_SYNC_STATUS_OK) {
      test_background_sync_manager_->DidResolveRegistration(
          blink::mojom::BackgroundSyncRegistrationInfo::New(
              sw_registration_id, options.tag, GetBackgroundSyncType(options)));
      base::RunLoop().RunUntilIdle();
    }

    return *callback_status == BACKGROUND_SYNC_STATUS_OK;
  }

  bool UnregisterWithServiceWorkerId(
      int64_t sw_registration_id,
      blink::mojom::SyncRegistrationOptions options) {
    if (GetBackgroundSyncType(options) ==
        blink::mojom::BackgroundSyncType::ONE_SHOT) {
      // Not supported for one-shot sync.
      return false;
    }

    bool was_called = false;
    test_background_sync_manager_->UnregisterPeriodicSync(
        sw_registration_id, options.tag,
        base::BindOnce(&BackgroundSyncManagerTest::StatusCallback,
                       base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    return callback_status_ == BACKGROUND_SYNC_STATUS_OK;
  }

  MockPermissionManager* GetPermissionControllerDelegate() {
    return static_cast<MockPermissionManager*>(
        helper_->browser_context()->GetPermissionControllerDelegate());
  }

  bool GetRegistration(
      blink::mojom::SyncRegistrationOptions registration_options) {
    if (GetBackgroundSyncType(registration_options) ==
        blink::mojom::BackgroundSyncType::ONE_SHOT) {
      return GetOneShotSyncRegistrationWithServiceWorkerId(
          sw_registration_id_1_, std::move(registration_options));
    }
    return GetPeriodicSyncRegistrationWithServiceWorkerId(
        sw_registration_id_1_, std::move(registration_options));
  }

  bool GetOneShotSyncRegistrationWithServiceWorkerId(
      int64_t sw_registration_id,
      blink::mojom::SyncRegistrationOptions registration_options) {
    bool was_called = false;

    test_background_sync_manager_->GetOneShotSyncRegistrations(
        sw_registration_id,
        base::BindOnce(&BackgroundSyncManagerTest::
                           StatusAndOneShotSyncRegistrationsCallback,
                       base::Unretained(this), &was_called));

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    if (one_shot_sync_callback_status_ == BACKGROUND_SYNC_STATUS_OK) {
      for (auto& one_shot_sync_registration :
           callback_one_shot_sync_registrations_) {
        if (one_shot_sync_registration->options()->Equals(
                registration_options)) {
          // Transfer the matching registration out of the vector into
          // |callback_one_shot_sync_registration_| for testing.
          callback_one_shot_sync_registration_ =
              std::move(one_shot_sync_registration);
          base::Erase(callback_one_shot_sync_registrations_,
                      one_shot_sync_registration);
          return true;
        }
      }
    }
    return false;
  }

  bool GetPeriodicSyncRegistrationWithServiceWorkerId(
      int64_t sw_registration_id,
      blink::mojom::SyncRegistrationOptions registration_options) {
    bool was_called = false;

    test_background_sync_manager_->GetPeriodicSyncRegistrations(
        sw_registration_id,
        base::BindOnce(&BackgroundSyncManagerTest::
                           StatusAndPeriodicSyncRegistrationsCallback,
                       base::Unretained(this), &was_called));

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    if (periodic_sync_callback_status_ == BACKGROUND_SYNC_STATUS_OK) {
      for (auto& periodic_sync_registration :
           callback_periodic_sync_registrations_) {
        if (periodic_sync_registration->options()->Equals(
                registration_options)) {
          // Transfer the matching registration out of the vector into
          // |callback_periodic_sync_registration_| for testing.
          callback_periodic_sync_registration_ =
              std::move(periodic_sync_registration);
          base::Erase(callback_periodic_sync_registrations_,
                      periodic_sync_registration);
          return true;
        }
      }
    }
    return false;
  }

  bool GetOneShotSyncRegistrations() {
    bool was_called = false;
    test_background_sync_manager_->GetOneShotSyncRegistrations(
        sw_registration_id_1_,
        base::BindOnce(&BackgroundSyncManagerTest::
                           StatusAndOneShotSyncRegistrationsCallback,
                       base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    return one_shot_sync_callback_status_ == BACKGROUND_SYNC_STATUS_OK;
  }

  bool GetPeriodicSyncRegistrations() {
    bool was_called = false;
    test_background_sync_manager_->GetPeriodicSyncRegistrations(
        sw_registration_id_1_,
        base::BindOnce(&BackgroundSyncManagerTest::
                           StatusAndPeriodicSyncRegistrationsCallback,
                       base::Unretained(this), &was_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);

    return periodic_sync_callback_status_ == BACKGROUND_SYNC_STATUS_OK;
  }

  MockBackgroundSyncController* GetController() {
    return static_cast<MockBackgroundSyncController*>(
        helper_->browser_context()->GetBackgroundSyncController());
  }

  void StorageRegistrationCallback(blink::ServiceWorkerStatusCode result) {
    callback_sw_status_code_ = result;
  }

  void UnregisterServiceWorker(uint64_t sw_registration_id) {
    bool called = false;
    helper_->context()->UnregisterServiceWorker(
        ScopeForSWId(sw_registration_id),
        base::BindOnce(&UnregisterServiceWorkerCallback, &called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
  }

  GURL ScopeForSWId(int64_t sw_id) {
    EXPECT_TRUE(sw_id == sw_registration_id_1_ ||
                sw_id == sw_registration_id_2_);
    return sw_id == sw_registration_id_1_ ? GURL(kScope1) : GURL(kScope2);
  }

  void SetupForSyncEvent(
      const TestBackgroundSyncManager::DispatchSyncCallback& callback) {
    test_background_sync_manager_->set_dispatch_sync_callback(callback);
    SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  }

  void SetupForPeriodicSyncEvent(
      const TestBackgroundSyncManager::DispatchSyncCallback& callback) {
    test_background_sync_manager_->set_dispatch_periodic_sync_callback(
        callback);
    SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  }

  void DispatchSyncStatusCallback(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerVersion> active_version,
      ServiceWorkerVersion::StatusCallback callback) {
    sync_events_called_++;
    std::move(callback).Run(status);
  }

  void DispatchPeriodicSyncStatusCallback(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerVersion> active_version,
      ServiceWorkerVersion::StatusCallback callback) {
    periodic_sync_events_called_++;
    std::move(callback).Run(status);
  }

  void InitSyncEventTest() {
    SetupForSyncEvent(base::BindRepeating(
        &BackgroundSyncManagerTest::DispatchSyncStatusCallback,
        base::Unretained(this), blink::ServiceWorkerStatusCode::kOk));
  }

  void InitPeriodicSyncEventTest() {
    SetupForPeriodicSyncEvent(base::BindRepeating(
        &BackgroundSyncManagerTest::DispatchPeriodicSyncStatusCallback,
        base::Unretained(this), blink::ServiceWorkerStatusCode::kOk));
  }

  void InitFailedSyncEventTest() {
    SetupForSyncEvent(base::BindRepeating(
        &BackgroundSyncManagerTest::DispatchSyncStatusCallback,
        base::Unretained(this),
        blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected));
  }

  void DispatchSyncDelayedCallback(
      scoped_refptr<ServiceWorkerVersion> active_version,
      ServiceWorkerVersion::StatusCallback callback) {
    sync_events_called_++;
    sync_fired_callback_ = std::move(callback);
  }

  void InitDelayedSyncEventTest() {
    SetupForSyncEvent(base::BindRepeating(
        &BackgroundSyncManagerTest::DispatchSyncDelayedCallback,
        base::Unretained(this)));
  }

  void RegisterAndVerifySyncEventDelayed(
      blink::mojom::SyncRegistrationOptions sync_options) {
    int count_sync_events = sync_events_called_;
    EXPECT_FALSE(sync_fired_callback_);

    EXPECT_TRUE(Register(sync_options));

    EXPECT_EQ(count_sync_events + 1, sync_events_called_);
    EXPECT_TRUE(GetRegistration(std::move(sync_options)));
    EXPECT_TRUE(sync_fired_callback_);
  }

  void DeleteServiceWorkerAndStartOver() {
    helper_->context()->ScheduleDeleteAndStartOver();
    content::RunAllTasksUntilIdle();
  }

  int MaxTagLength() const { return BackgroundSyncManager::kMaxTagLength; }

  void SetMaxSyncAttemptsAndRestartManager(int max_sync_attempts) {
    BackgroundSyncParameters* parameters =
        GetController()->background_sync_parameters();
    parameters->max_sync_attempts = max_sync_attempts;
    parameters->max_sync_attempts_with_notification_permission =
        max_sync_attempts + 1;

    // Restart the BackgroundSyncManager so that it updates its parameters.
    SetupBackgroundSyncManager();
  }

  void FireReadyEvents() { test_background_sync_manager_->OnNetworkChanged(); }

  bool AreOptionConditionsMet() {
    return test_background_sync_manager_->AreOptionConditionsMet();
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::unique_ptr<StoragePartitionImpl> storage_partition_impl_;
  std::unique_ptr<TestBackgroundSyncManager> test_background_sync_manager_ =
      nullptr;
  base::SimpleTestClock test_clock_;

  int64_t sw_registration_id_1_;
  int64_t sw_registration_id_2_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_1_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_2_;

  blink::mojom::SyncRegistrationOptions sync_options_1_;
  blink::mojom::SyncRegistrationOptions sync_options_2_;

  // Callback values.
  BackgroundSyncStatus one_shot_sync_callback_status_ =
      BACKGROUND_SYNC_STATUS_OK;
  BackgroundSyncStatus periodic_sync_callback_status_ =
      BACKGROUND_SYNC_STATUS_OK;
  BackgroundSyncStatus callback_status_ = BACKGROUND_SYNC_STATUS_OK;
  std::unique_ptr<BackgroundSyncRegistration>
      callback_one_shot_sync_registration_;
  std::unique_ptr<BackgroundSyncRegistration>
      callback_periodic_sync_registration_;
  std::vector<std::unique_ptr<BackgroundSyncRegistration>>
      callback_one_shot_sync_registrations_;
  std::vector<std::unique_ptr<BackgroundSyncRegistration>>
      callback_periodic_sync_registrations_;
  blink::ServiceWorkerStatusCode callback_sw_status_code_ =
      blink::ServiceWorkerStatusCode::kOk;
  int sync_events_called_ = 0;
  int periodic_sync_events_called_ = 0;
  ServiceWorkerVersion::StatusCallback sync_fired_callback_;
};

TEST_F(BackgroundSyncManagerTest, Register) {
  EXPECT_TRUE(Register(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, Unregister) {
  // Not supported for One-shot syncs.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_FALSE(Unregister(sync_options_1_));

  sync_options_1_.min_interval = 36000;
  EXPECT_TRUE(Register(sync_options_1_));

  // Don't fail for non-existent Periodic Sync registrations.
  sync_options_2_.min_interval = 36000;
  EXPECT_TRUE(Unregister(sync_options_2_));

  // Unregistering one periodic sync doesn't affect another.
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(Unregister(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  // Disable manager. Unregister should fail.
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Unregister(sync_options_2_));
  SetupBackgroundSyncManager();
  EXPECT_TRUE(Unregister(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, UnregistrationStopsPeriodicTasks) {
  InitPeriodicSyncEventTest();
  int thirteen_hours_ms = 13 * 60 * 60 * 1000;
  sync_options_2_.min_interval = thirteen_hours_ms;

  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_EQ(0, periodic_sync_events_called_);

  // Advance clock.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(thirteen_hours_ms));
  FireReadyEvents();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, periodic_sync_events_called_);

  EXPECT_TRUE(Unregister(sync_options_2_));

  // Advance clock. Expect no increase in periodicSync events fired.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(thirteen_hours_ms));
  FireReadyEvents();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, periodic_sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, RegisterAndWaitToFireUntilResolved) {
  InitSyncEventTest();
  bool was_called = false;
  test_background_sync_manager_->Register(
      sw_registration_id_1_, sync_options_1_,
      base::BindOnce(
          &BackgroundSyncManagerTest::StatusAndOneShotSyncRegistrationCallback,
          base::Unretained(this), &was_called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);

  // Verify that the sync event hasn't fired yet, as it should wait for the
  // client to acknowledge with DidResolveRegistration.
  EXPECT_EQ(0, sync_events_called_);

  test_background_sync_manager_->DidResolveRegistration(
      blink::mojom::BackgroundSyncRegistrationInfo::New(
          sw_registration_id_1_, sync_options_1_.tag,
          GetBackgroundSyncType(sync_options_1_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, ResolveInvalidRegistration) {
  InitSyncEventTest();
  bool was_called = false;
  test_background_sync_manager_->Register(
      sw_registration_id_1_, sync_options_1_,
      base::BindOnce(
          &BackgroundSyncManagerTest::StatusAndOneShotSyncRegistrationCallback,
          base::Unretained(this), &was_called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);

  // Verify that the sync event hasn't fired yet, as it should wait for the
  // client to acknowledge with DidResolveRegistration.
  EXPECT_EQ(0, sync_events_called_);

  // Resolve a non-existing registration.
  test_background_sync_manager_->DidResolveRegistration(
      blink::mojom::BackgroundSyncRegistrationInfo::New(
          sw_registration_id_1_, "unknown_tag",
          GetBackgroundSyncType(sync_options_1_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, RegistrationIntact) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(sync_options_1_.tag,
            callback_one_shot_sync_registration_->options()->tag);
  sync_options_2_.min_interval = 3600;
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_EQ(sync_options_2_.tag,
            callback_periodic_sync_registration_->options()->tag);
}

TEST_F(BackgroundSyncManagerTest, RegisterWithoutLiveSWRegistration) {
  // Get a provider host which is used to install the service worker.
  ASSERT_TRUE(sw_registration_1_->active_version());
  ASSERT_FALSE(sw_registration_1_->waiting_version());
  ASSERT_FALSE(sw_registration_1_->installing_version());
  ServiceWorkerProviderHost* provider_host =
      sw_registration_1_->active_version()->provider_host();
  ASSERT_TRUE(provider_host);

  // Remove the registration object host.
  provider_host->registration_object_hosts_.clear();

  // Ensure |sw_registration_1_| is the last reference to the registration.
  ASSERT_TRUE(sw_registration_1_->HasOneRef());
  sw_registration_1_ = nullptr;

  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
            one_shot_sync_callback_status_);
}

TEST_F(BackgroundSyncManagerTest, RegisterWithoutActiveSWRegistration) {
  sw_registration_1_->UnsetVersion(sw_registration_1_->active_version());
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
            one_shot_sync_callback_status_);
}

TEST_F(BackgroundSyncManagerTest, RegisterBadBackend) {
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, RegisterPermissionDenied) {
  GURL expected_origin = GURL(kScope1).GetOrigin();
  MockPermissionManager* mock_permission_manager =
      GetPermissionControllerDelegate();

  EXPECT_CALL(*mock_permission_manager,
              GetPermissionStatus(PermissionType::NOTIFICATIONS,
                                  expected_origin, expected_origin))
      .Times(2);

  EXPECT_CALL(*mock_permission_manager,
              GetPermissionStatus(PermissionType::BACKGROUND_SYNC,
                                  expected_origin, expected_origin))
      .WillOnce(testing::Return(blink::mojom::PermissionStatus::DENIED));
  EXPECT_FALSE(Register(sync_options_1_));

  sync_options_2_.min_interval = 36000;
  EXPECT_CALL(*mock_permission_manager,
              GetPermissionStatus(PermissionType::PERIODIC_BACKGROUND_SYNC,
                                  expected_origin, expected_origin))
      .WillOnce(testing::Return(blink::mojom::PermissionStatus::DENIED));
  EXPECT_FALSE(Register(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, RegisterPermissionGranted) {
  GURL expected_origin = GURL(kScope1).GetOrigin();
  MockPermissionManager* mock_permission_manager =
      GetPermissionControllerDelegate();

  EXPECT_CALL(*mock_permission_manager,
              GetPermissionStatus(PermissionType::NOTIFICATIONS,
                                  expected_origin, expected_origin))
      .Times(2);

  EXPECT_CALL(*mock_permission_manager,
              GetPermissionStatus(PermissionType::BACKGROUND_SYNC,
                                  expected_origin, expected_origin))
      .WillOnce(testing::Return(blink::mojom::PermissionStatus::GRANTED));
  EXPECT_TRUE(Register(sync_options_1_));

  sync_options_2_.min_interval = 36000;
  EXPECT_CALL(*mock_permission_manager,
              GetPermissionStatus(PermissionType::PERIODIC_BACKGROUND_SYNC,
                                  expected_origin, expected_origin))
      .WillOnce(testing::Return(blink::mojom::PermissionStatus::GRANTED));
  EXPECT_TRUE(Register(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, TwoRegistrations) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationNonExisting) {
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationExisting) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationBadBackend) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(Register(sync_options_2_));
  // Registration should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsZero) {
  EXPECT_TRUE(GetOneShotSyncRegistrations());
  EXPECT_EQ(0u, callback_one_shot_sync_registrations_.size());
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsOne) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetOneShotSyncRegistrations());

  ASSERT_EQ(1u, callback_one_shot_sync_registrations_.size());
  sync_options_1_.Equals(*callback_one_shot_sync_registrations_[0]->options());

  sync_options_1_.min_interval = 3600;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetPeriodicSyncRegistrations());

  ASSERT_EQ(1u, callback_periodic_sync_registrations_.size());
  sync_options_1_.Equals(*callback_periodic_sync_registrations_[0]->options());
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsTwo) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(GetOneShotSyncRegistrations());

  ASSERT_EQ(2u, callback_one_shot_sync_registrations_.size());
  sync_options_1_.Equals(*callback_one_shot_sync_registrations_[0]->options());
  sync_options_2_.Equals(*callback_one_shot_sync_registrations_[1]->options());

  sync_options_1_.min_interval = 3600;
  sync_options_2_.min_interval = 3600;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(GetPeriodicSyncRegistrations());

  ASSERT_EQ(2u, callback_periodic_sync_registrations_.size());
  sync_options_1_.Equals(*callback_periodic_sync_registrations_[0]->options());
  sync_options_2_.Equals(*callback_periodic_sync_registrations_[1]->options());
}

TEST_F(BackgroundSyncManagerTest, GetRegistrationsBadBackend) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_TRUE(GetOneShotSyncRegistrations());
  EXPECT_FALSE(Register(sync_options_2_));
  // Registration should have discovered the bad backend and disabled the
  // BackgroundSyncManager.
  EXPECT_FALSE(GetOneShotSyncRegistrations());
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetOneShotSyncRegistrations());
}


TEST_F(BackgroundSyncManagerTest, Reregister) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, ReregisterSecond) {
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(Register(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, ReregisterPeriodicSync) {
  sync_options_1_.tag = sync_options_2_.tag;
  sync_options_1_.min_interval = 1000;
  sync_options_2_.min_interval = 2000;

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, RegisterMaxTagLength) {
  sync_options_1_.tag = std::string(MaxTagLength(), 'a');
  EXPECT_TRUE(Register(sync_options_1_));

  sync_options_2_.tag = std::string(MaxTagLength() + 1, 'b');
  EXPECT_FALSE(Register(sync_options_2_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_NOT_ALLOWED, one_shot_sync_callback_status_);
}

TEST_F(BackgroundSyncManagerTest, RebootRecovery) {
  EXPECT_TRUE(Register(sync_options_1_));

  SetupBackgroundSyncManager();

  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, RebootRecoveryTwoServiceWorkers) {
  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_1_, sync_options_1_));
  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_2_, sync_options_2_));

  SetupBackgroundSyncManager();

  EXPECT_TRUE(GetOneShotSyncRegistrationWithServiceWorkerId(
      sw_registration_id_1_, sync_options_1_));
  EXPECT_FALSE(GetOneShotSyncRegistrationWithServiceWorkerId(
      sw_registration_id_1_, sync_options_2_));
  EXPECT_FALSE(GetOneShotSyncRegistrationWithServiceWorkerId(
      sw_registration_id_2_, sync_options_1_));
  EXPECT_TRUE(GetOneShotSyncRegistrationWithServiceWorkerId(
      sw_registration_id_2_, sync_options_2_));

  EXPECT_TRUE(GetOneShotSyncRegistrationWithServiceWorkerId(
      sw_registration_id_1_, sync_options_1_));
  EXPECT_TRUE(GetOneShotSyncRegistrationWithServiceWorkerId(
      sw_registration_id_2_, sync_options_2_));

  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_1_, sync_options_2_));
  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_2_, sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, InitWithBadBackend) {
  SetupCorruptBackgroundSyncManager();

  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, SequentialOperations) {
  // Schedule Init and all of the operations on a delayed backend. Verify that
  // the operations complete sequentially.
  SetupDelayedBackgroundSyncManager();

  bool register_called = false;
  bool get_registrations_called = false;
  test_background_sync_manager_->Register(
      sw_registration_id_1_, sync_options_1_,
      base::BindOnce(
          &BackgroundSyncManagerTest::StatusAndOneShotSyncRegistrationCallback,
          base::Unretained(this), &register_called));
  test_background_sync_manager_->GetOneShotSyncRegistrations(
      sw_registration_id_1_,
      base::BindOnce(
          &BackgroundSyncManagerTest::StatusAndOneShotSyncRegistrationsCallback,
          base::Unretained(this), &get_registrations_called));

  base::RunLoop().RunUntilIdle();
  // Init should be blocked while loading from the backend.
  EXPECT_FALSE(register_called);
  EXPECT_FALSE(get_registrations_called);

  test_background_sync_manager_->ResumeBackendOperation();
  base::RunLoop().RunUntilIdle();
  // Register should be blocked while storing to the backend.
  EXPECT_FALSE(register_called);
  EXPECT_FALSE(get_registrations_called);

  test_background_sync_manager_->ResumeBackendOperation();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(register_called);
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_OK, one_shot_sync_callback_status_);
  // GetRegistrations should run immediately as it doesn't write to disk.
  EXPECT_TRUE(get_registrations_called);
}

TEST_F(BackgroundSyncManagerTest, UnregisterServiceWorker) {
  EXPECT_TRUE(Register(sync_options_1_));
  UnregisterServiceWorker(sw_registration_id_1_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest,
       UnregisterServiceWorkerDuringSyncRegistration) {
  EXPECT_TRUE(Register(sync_options_1_));
  sync_options_2_.min_interval = 3600;

  test_background_sync_manager_->set_delay_backend(true);
  bool callback_called = false;
  test_background_sync_manager_->Register(
      sw_registration_id_1_, sync_options_2_,
      base::BindOnce(
          &BackgroundSyncManagerTest::StatusAndPeriodicSyncRegistrationCallback,
          base::Unretained(this), &callback_called));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_called);
  UnregisterServiceWorker(sw_registration_id_1_);

  test_background_sync_manager_->ResumeBackendOperation();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
            periodic_sync_callback_status_);

  test_background_sync_manager_->set_delay_backend(false);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DeleteAndStartOverServiceWorkerContext) {
  EXPECT_TRUE(Register(sync_options_1_));
  DeleteServiceWorkerAndStartOver();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DisabledManagerWorksAfterBrowserRestart) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_2_));

  // The manager is now disabled and not accepting new requests until browser
  // restart or notification that the storage has been wiped.
  test_background_sync_manager_->set_corrupt_backend(false);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(Register(sync_options_2_));

  // Simulate restarting the browser by creating a new BackgroundSyncManager.
  SetupBackgroundSyncManager();
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, DisabledManagerWorksAfterDeleteAndStartOver) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_2_));

  // The manager is now disabled and not accepting new requests until browser
  // restart or notification that the storage has been wiped.
  test_background_sync_manager_->set_corrupt_backend(false);
  DeleteServiceWorkerAndStartOver();

  RegisterServiceWorkers();

  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, RegistrationEqualsTag) {
  BackgroundSyncRegistration reg_1;
  BackgroundSyncRegistration reg_2;
  EXPECT_TRUE(reg_1.Equals(reg_2));
  reg_2.options()->tag = "bar";
  EXPECT_FALSE(reg_1.Equals(reg_2));
}

TEST_F(BackgroundSyncManagerTest, StoreAndRetrievePreservesValues) {
  InitDelayedSyncEventTest();
  blink::mojom::SyncRegistrationOptions options;

  // Set non-default values for each field.
  options.tag = "foo";

  // Store the registration.
  EXPECT_TRUE(Register(options));

  // Simulate restarting the sync manager, forcing the next read to come from
  // disk.
  SetupBackgroundSyncManager();

  EXPECT_TRUE(GetRegistration(options));
  EXPECT_TRUE(options.Equals(*callback_one_shot_sync_registration_->options()));
}

TEST_F(BackgroundSyncManagerTest, EmptyTagSupported) {
  sync_options_1_.tag = "";
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(
      sync_options_1_.Equals(*callback_one_shot_sync_registration_->options()));
}

TEST_F(BackgroundSyncManagerTest, PeriodicSyncOptions) {
  sync_options_1_.min_interval = 2;
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(
      sync_options_1_.Equals(*callback_periodic_sync_registration_->options()));
}

TEST_F(BackgroundSyncManagerTest, BothTypesOfSyncShareATag) {
  sync_options_1_.tag = "foo";
  sync_options_2_.tag = "foo";
  // Make the registration periodic.
  sync_options_2_.min_interval = 36000;

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(callback_one_shot_sync_registration_->options()->tag, "foo");
  EXPECT_TRUE(
      sync_options_1_.Equals(*callback_one_shot_sync_registration_->options()));

  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));
  EXPECT_TRUE(
      sync_options_2_.Equals(*callback_periodic_sync_registration_->options()));
  EXPECT_EQ(callback_periodic_sync_registration_->options()->tag, "foo");
}

TEST_F(BackgroundSyncManagerTest, FiresOnRegistration) {
  InitSyncEventTest();

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, PeriodicSyncFiresWhenExpected) {
  InitPeriodicSyncEventTest();
  int thirteen_hours_ms = 13 * 60 * 60 * 1000;
  sync_options_2_.min_interval = thirteen_hours_ms;

  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_EQ(0, periodic_sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  // Advance clock.
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(thirteen_hours_ms));
  FireReadyEvents();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, periodic_sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, ReregisterMidSyncFirstAttemptFails) {
  InitDelayedSyncEventTest();
  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  // Reregister the event mid-sync
  EXPECT_TRUE(Register(sync_options_1_));

  // The first sync attempt fails.
  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_)
      .Run(blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected);
  base::RunLoop().RunUntilIdle();

  // It should fire again since it was reregistered mid-sync.
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_).Run(blink::ServiceWorkerStatusCode::kOk);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, ReregisterMidSyncFirstAttemptSucceeds) {
  InitDelayedSyncEventTest();
  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  // Reregister the event mid-sync
  EXPECT_TRUE(Register(sync_options_1_));

  // The first sync event succeeds.
  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_).Run(blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  // It should fire again since it was reregistered mid-sync.
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_).Run(blink::ServiceWorkerStatusCode::kOk);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, OverwritePendingRegistration) {
  InitFailedSyncEventTest();

  // Prevent the first sync from running so that it stays in a pending state.
  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Overwrite the first sync. It should still be pending.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Verify that it only gets to run once.
  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DisableWhilePending) {
  InitDelayedSyncEventTest();
  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Register(sync_options_1_));

  // Corrupting the backend should result in the manager disabling itself on the
  // next operation.
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_2_));

  test_background_sync_manager_->set_corrupt_backend(false);
  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, DisableWhileFiring) {
  InitDelayedSyncEventTest();

  // Register a one-shot that pauses mid-fire.
  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  // Corrupting the backend should result in the manager disabling itself on the
  // next operation.
  test_background_sync_manager_->set_corrupt_backend(true);
  EXPECT_FALSE(Register(sync_options_2_));
  test_background_sync_manager_->set_corrupt_backend(false);

  // Successfully complete the firing event. We can't verify that it actually
  // completed but at least we can test that it doesn't crash.
  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_).Run(blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();
}

TEST_F(BackgroundSyncManagerTest, FiresOnNetworkChange) {
  InitSyncEventTest();

  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, MultipleRegistrationsFireOnNetworkChange) {
  InitSyncEventTest();

  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);

  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, FiresOnManagerRestart) {
  InitSyncEventTest();

  // Initially the event won't run because there is no network.
  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Simulate closing the browser.
  DeleteBackgroundSyncManager();

  // The next time the manager is started, the network is good.
  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  SetupBackgroundSyncManager();
  InitSyncEventTest();

  // The event should have fired.
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, FailedRegistrationShouldBeRemoved) {
  InitFailedSyncEventTest();

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, FailedRegistrationReregisteredAndFires) {
  InitFailedSyncEventTest();

  // The initial sync event fails.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));

  InitSyncEventTest();

  // Reregistering should cause the sync event to fire again, this time
  // succeeding.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DelayMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  // Finish firing the event and verify that the registration is removed.
  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_).Run(blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, BadBackendMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  test_background_sync_manager_->set_corrupt_backend(true);
  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_).Run(blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  // The backend should now be disabled because it couldn't unregister the
  // one-shot.
  EXPECT_FALSE(Register(sync_options_2_));
  EXPECT_FALSE(
      RegisterWithServiceWorkerId(sw_registration_id_2_, sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, UnregisterServiceWorkerMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  UnregisterServiceWorker(sw_registration_id_1_);

  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_).Run(blink::ServiceWorkerStatusCode::kOk);

  // The backend isn't disabled, but the first service worker registration is
  // gone.
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_TRUE(
      RegisterWithServiceWorkerId(sw_registration_id_2_, sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, KillManagerMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);

  // Create a new manager which should fire the sync again on init.
  SetupBackgroundSyncManager();
  InitSyncEventTest();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_EQ(2, sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, RegisterWithoutMainFrame) {
  test_background_sync_manager_->set_has_main_frame_provider_host(false);
  EXPECT_FALSE(Register(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, RegisterExistingWithoutMainFrame) {
  EXPECT_TRUE(Register(sync_options_1_));
  test_background_sync_manager_->set_has_main_frame_provider_host(false);
  EXPECT_FALSE(Register(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DefaultParameters) {
  *GetController()->background_sync_parameters() = BackgroundSyncParameters();
  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();

  EXPECT_EQ(BackgroundSyncParameters(),
            *test_background_sync_manager_->background_sync_parameters());
}

TEST_F(BackgroundSyncManagerTest, OverrideParameters) {
  BackgroundSyncParameters* parameters =
      GetController()->background_sync_parameters();
  parameters->disable = true;
  parameters->max_sync_attempts = 100;
  parameters->initial_retry_delay = base::TimeDelta::FromMinutes(200);
  parameters->retry_delay_factor = 300;
  parameters->min_sync_recovery_time = base::TimeDelta::FromMinutes(400);
  parameters->max_sync_event_duration = base::TimeDelta::FromMinutes(500);

  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();

  // Check that the manager is disabled
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
            one_shot_sync_callback_status_);

  const BackgroundSyncParameters* manager_parameters =
      test_background_sync_manager_->background_sync_parameters();
  EXPECT_EQ(*parameters, *manager_parameters);
}

TEST_F(BackgroundSyncManagerTest, DisablingFromControllerKeepsRegistrations) {
  EXPECT_TRUE(Register(sync_options_1_));

  BackgroundSyncParameters* parameters =
      GetController()->background_sync_parameters();
  parameters->disable = true;

  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();
  EXPECT_FALSE(GetRegistration(sync_options_1_));  // fails because disabled

  // Reenable the BackgroundSyncManager on next launch
  parameters->disable = false;

  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();
  EXPECT_TRUE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, DisabledPermanently) {
  BackgroundSyncParameters* parameters =
      GetController()->background_sync_parameters();
  parameters->disable = true;

  // Restart the BackgroundSyncManager so that it updates its parameters.
  SetupBackgroundSyncManager();

  // Check that the manager is disabled
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
            one_shot_sync_callback_status_);

  // If the service worker is wiped and the manager is restarted, the manager
  // should stay disabled.
  DeleteServiceWorkerAndStartOver();
  RegisterServiceWorkers();
  EXPECT_FALSE(Register(sync_options_1_));
  EXPECT_EQ(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
            one_shot_sync_callback_status_);
}

TEST_F(BackgroundSyncManagerTest, NotifyBackgroundSyncRegistered) {
  // Verify that the BackgroundSyncController is informed of registrations.
  EXPECT_EQ(0, GetController()->registration_count());
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(1, GetController()->registration_count());
  EXPECT_EQ(url::Origin::Create(GURL(kScope1)),
            GetController()->registration_origin());
}

TEST_F(BackgroundSyncManagerTest, WakeBrowserCalled) {
  SetupBackgroundSyncManager();
  InitDelayedSyncEventTest();

  // The BackgroundSyncManager should declare in initialization
  // that it doesn't need to be woken up since it has no registrations.
  EXPECT_LT(0, GetController()->run_in_background_count());
  EXPECT_FALSE(test_background_sync_manager_->IsBrowserWakeupScheduled());

  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(test_background_sync_manager_->IsBrowserWakeupScheduled());

  // Register a one-shot but it can't fire due to lack of network, wake up is
  // required.
  Register(sync_options_1_);
  EXPECT_TRUE(test_background_sync_manager_->IsBrowserWakeupScheduled());

  // Start the event but it will pause mid-sync due to
  // InitDelayedSyncEventTest() above.
  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(test_background_sync_manager_->IsBrowserWakeupScheduled());
  EXPECT_TRUE(test_background_sync_manager_->EqualsSoonestOneShotWakeupDelta(
      test_background_sync_manager_->background_sync_parameters()
          ->min_sync_recovery_time));

  // Finish the sync.
  ASSERT_TRUE(sync_fired_callback_);
  std::move(sync_fired_callback_).Run(blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_background_sync_manager_->IsBrowserWakeupScheduled());
}

TEST_F(BackgroundSyncManagerTest, GetSoonestWakeupDeltaConsidersSyncType) {
  // Register a one-shot sync.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Also register a Periodic sync.
  sync_options_2_.min_interval = 13 * 60 * 60 * 1000;
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  EXPECT_EQ(GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta());
  EXPECT_EQ(GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::PERIODIC),
            base::TimeDelta::FromMilliseconds(sync_options_2_.min_interval));
}

TEST_F(BackgroundSyncManagerTest, SoonestWakeupDeltaDecreasesWithTime) {
  // Register a periodic sync.
  int thirteen_hours_ms = 13 * 60 * 60 * 1000;
  sync_options_2_.min_interval = thirteen_hours_ms * 4;
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  base::TimeDelta soonest_wakeup_delta_1 =
      GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::PERIODIC);

  test_clock_.Advance(base::TimeDelta::FromMilliseconds(thirteen_hours_ms));
  base::TimeDelta soonest_wakeup_delta_2 =
      GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::PERIODIC);

  test_clock_.Advance(base::TimeDelta::FromMilliseconds(thirteen_hours_ms));
  base::TimeDelta soonest_wakeup_delta_3 =
      GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::PERIODIC);

  test_clock_.Advance(base::TimeDelta::FromMilliseconds(thirteen_hours_ms));
  base::TimeDelta soonest_wakeup_delta_4 =
      GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::PERIODIC);

  EXPECT_GT(soonest_wakeup_delta_1, soonest_wakeup_delta_2);
  EXPECT_GT(soonest_wakeup_delta_2, soonest_wakeup_delta_3);
  EXPECT_GT(soonest_wakeup_delta_3, soonest_wakeup_delta_4);
}

TEST_F(BackgroundSyncManagerTest, OneAttempt) {
  SetMaxSyncAttemptsAndRestartManager(1);
  InitFailedSyncEventTest();

  // It should permanently fail after failing once.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, TwoAttempts) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(test_background_sync_manager_->IsDelayedTaskScheduled());

  // Make sure the delay is reasonable.
  EXPECT_LT(base::TimeDelta::FromMinutes(1),
            test_background_sync_manager_->delayed_task_delta());
  EXPECT_GT(base::TimeDelta::FromHours(1),
            test_background_sync_manager_->delayed_task_delta());

  // Fire again and this time it should permanently fail.
  test_clock_.Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->RunDelayedTask();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, ThreeAttempts) {
  SetMaxSyncAttemptsAndRestartManager(3);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(test_background_sync_manager_->IsDelayedTaskScheduled());

  // The second run will fail but it will setup a timer to try again.
  base::TimeDelta first_delta =
      test_background_sync_manager_->delayed_task_delta();
  test_clock_.Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->RunDelayedTask();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Verify that the delta grows for each attempt.
  EXPECT_LT(first_delta, test_background_sync_manager_->delayed_task_delta());

  // The third run will permanently fail.
  test_clock_.Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->RunDelayedTask();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, WaitsFullDelayTime) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(test_background_sync_manager_->IsDelayedTaskScheduled());

  // Fire again one second before it's ready to retry. Expect it to reschedule
  // the delay timer for one more second.
  test_clock_.Advance(test_background_sync_manager_->delayed_task_delta() -
                      base::TimeDelta::FromSeconds(1));
  test_background_sync_manager_->RunDelayedTask();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            test_background_sync_manager_->delayed_task_delta());

  // Fire one second later and it should fail permanently.
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  test_background_sync_manager_->RunDelayedTask();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, RetryOnBrowserRestart) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Simulate restarting the browser after sufficient time has passed.
  base::TimeDelta delta = test_background_sync_manager_->delayed_task_delta();
  CreateBackgroundSyncManager();
  InitFailedSyncEventTest();
  test_clock_.Advance(delta);
  InitBackgroundSyncManager();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, RescheduleOnBrowserRestart) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Simulate restarting the browser before the retry timer has expired.
  base::TimeDelta delta = test_background_sync_manager_->delayed_task_delta();
  CreateBackgroundSyncManager();
  InitFailedSyncEventTest();
  test_clock_.Advance(delta - base::TimeDelta::FromSeconds(1));
  InitBackgroundSyncManager();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            test_background_sync_manager_->delayed_task_delta());
}

TEST_F(BackgroundSyncManagerTest, RetryIfClosedMidSync) {
  InitDelayedSyncEventTest();

  RegisterAndVerifySyncEventDelayed(sync_options_1_);
  // The time delta is the recovery timer.
  base::TimeDelta delta = test_background_sync_manager_->delayed_task_delta();

  // Simulate restarting the browser after the recovery time, the event should
  // fire once and then fail permanently.
  CreateBackgroundSyncManager();
  InitFailedSyncEventTest();
  test_clock_.Advance(delta);
  InitBackgroundSyncManager();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
}

TEST_F(BackgroundSyncManagerTest, AllTestsEventuallyFire) {
  SetMaxSyncAttemptsAndRestartManager(3);
  InitFailedSyncEventTest();

  // The first run will fail but it will setup a timer to try again.
  EXPECT_TRUE(Register(sync_options_1_));

  // Run it a second time.
  test_clock_.Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->RunDelayedTask();
  base::RunLoop().RunUntilIdle();

  base::TimeDelta delay_delta =
      test_background_sync_manager_->delayed_task_delta();

  // Create a second registration, which will fail and setup a timer.
  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_GT(delay_delta, test_background_sync_manager_->delayed_task_delta());

  while (test_background_sync_manager_->IsDelayedTaskScheduled()) {
    test_clock_.Advance(test_background_sync_manager_->delayed_task_delta());
    test_background_sync_manager_->RunDelayedTask();
    EXPECT_FALSE(test_background_sync_manager_->IsDelayedTaskScheduled());
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, LastChance) {
  SetMaxSyncAttemptsAndRestartManager(2);
  InitFailedSyncEventTest();

  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_FALSE(test_background_sync_manager_->last_chance());
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  // Run it again.
  test_clock_.Advance(test_background_sync_manager_->delayed_task_delta());
  test_background_sync_manager_->RunDelayedTask();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(test_background_sync_manager_->last_chance());
}

TEST_F(BackgroundSyncManagerTest, EmulateOfflineSingleClient) {
  InitSyncEventTest();

  test_background_sync_manager_->EmulateServiceWorkerOffline(
      sw_registration_id_1_, true);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  test_background_sync_manager_->EmulateServiceWorkerOffline(
      sw_registration_id_1_, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));

  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

TEST_F(BackgroundSyncManagerTest, EmulateOfflineMultipleClients) {
  InitSyncEventTest();

  test_background_sync_manager_->EmulateServiceWorkerOffline(
      sw_registration_id_1_, true);
  EXPECT_TRUE(Register(sync_options_1_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));

  test_background_sync_manager_->EmulateServiceWorkerOffline(
      sw_registration_id_1_, true);

  EXPECT_TRUE(Register(sync_options_2_));
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  test_background_sync_manager_->EmulateServiceWorkerOffline(
      sw_registration_id_1_, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, sync_events_called_);
  EXPECT_TRUE(GetRegistration(sync_options_1_));
  EXPECT_TRUE(GetRegistration(sync_options_2_));

  test_background_sync_manager_->EmulateServiceWorkerOffline(
      sw_registration_id_1_, false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, sync_events_called_);
  EXPECT_FALSE(GetRegistration(sync_options_1_));
  EXPECT_FALSE(GetRegistration(sync_options_2_));
}

static void EmulateDispatchSyncEventCallback(
    bool* was_called,
    blink::ServiceWorkerStatusCode* code,
    blink::ServiceWorkerStatusCode status_code) {
  *was_called = true;
  *code = status_code;
}

TEST_F(BackgroundSyncManagerTest, EmulateDispatchSyncEvent) {
  InitSyncEventTest();
  bool was_called = false;
  blink::ServiceWorkerStatusCode code =
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected;
  test_background_sync_manager_->EmulateDispatchSyncEvent(
      "emulated_tag", sw_registration_1_->active_version(), false,
      base::BindOnce(EmulateDispatchSyncEventCallback, &was_called, &code));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, code);

  EXPECT_EQ(1, sync_events_called_);

  test_background_sync_manager_->EmulateServiceWorkerOffline(
      sw_registration_id_1_, true);

  was_called = false;
  test_background_sync_manager_->EmulateDispatchSyncEvent(
      "emulated_tag", sw_registration_1_->active_version(), false,
      base::BindOnce(EmulateDispatchSyncEventCallback, &was_called, &code));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected, code);

  test_background_sync_manager_->EmulateServiceWorkerOffline(
      sw_registration_id_1_, false);

  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  was_called = false;
  code = blink::ServiceWorkerStatusCode::kOk;
  test_background_sync_manager_->EmulateDispatchSyncEvent(
      "emulated_tag", sw_registration_1_->active_version(), false,
      base::BindOnce(EmulateDispatchSyncEventCallback, &was_called, &code));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected, code);

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  was_called = false;
  test_background_sync_manager_->EmulateDispatchSyncEvent(
      "emulated_tag", sw_registration_1_->active_version(), false,
      base::BindOnce(EmulateDispatchSyncEventCallback, &was_called, &code));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, code);

  EXPECT_EQ(2, sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, DispatchPeriodicSyncEvent) {
  InitPeriodicSyncEventTest();

  EXPECT_TRUE(AreOptionConditionsMet());

  bool was_called = false;
  blink::ServiceWorkerStatusCode code =
      blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected;
  test_background_sync_manager_->DispatchPeriodicSyncEvent(
      "test_tag", sw_registration_1_->active_version(),
      base::BindOnce(EmulateDispatchSyncEventCallback, &was_called, &code));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, code);

  EXPECT_EQ(1, periodic_sync_events_called_);
}

TEST_F(BackgroundSyncManagerTest, EventsLoggedForRegistration) {
  // Note that the dispatch is mocked out, so those events are not registered
  // by these tests.
  storage_partition_impl_->GetDevToolsBackgroundServicesContext()
      ->StartRecording(devtools::proto::BACKGROUND_SYNC);

  SetMaxSyncAttemptsAndRestartManager(3);
  InitFailedSyncEventTest();

  {
    // We expect a "Registered" event and a "Fail" event.
    EXPECT_CALL(*this, OnEventReceived(_)).Times(2);
    // The first run will fail but it will setup a timer to try again.
    EXPECT_TRUE(Register(sync_options_1_));
    EXPECT_TRUE(GetRegistration(sync_options_1_));
    EXPECT_TRUE(test_background_sync_manager_->IsDelayedTaskScheduled());
  }

  test_clock_.Advance(test_background_sync_manager_->delayed_task_delta());
  {
    // Expect another "Fail" event.
    EXPECT_CALL(*this, OnEventReceived(_)).Times(1);
    test_background_sync_manager_->RunDelayedTask();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(GetRegistration(sync_options_1_));
  }

  // The event should succeed now.
  InitSyncEventTest();

  test_clock_.Advance(test_background_sync_manager_->delayed_task_delta());
  {
    // Expect a "Completion" event.
    EXPECT_CALL(*this, OnEventReceived(_)).Times(1);
    test_background_sync_manager_->RunDelayedTask();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(GetRegistration(sync_options_1_));
  }
}

TEST_F(BackgroundSyncManagerTest, UkmRecordedAtCompletion) {
  InitSyncEventTest();
  {
    base::HistogramTester histogram_tester;

    EXPECT_TRUE(Register(sync_options_1_));

    test_background_sync_manager_->RunDelayedTask();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(GetRegistration(sync_options_1_));

    histogram_tester.ExpectBucketCount(
        "BackgroundSync.Registration.OneShot.EventSucceededAtCompletion", true,
        1);
    histogram_tester.ExpectBucketCount(
        "BackgroundSync.Registration.OneShot.NumAttemptsForSuccessfulEvent", 1,
        1);
  }

  SetMaxSyncAttemptsAndRestartManager(1);
  InitFailedSyncEventTest();
  {
    base::HistogramTester histogram_tester;

    EXPECT_TRUE(Register(sync_options_2_));

    test_background_sync_manager_->RunDelayedTask();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(GetRegistration(sync_options_2_));

    histogram_tester.ExpectBucketCount(
        "BackgroundSync.Registration.OneShot.EventSucceededAtCompletion", false,
        1);
    histogram_tester.ExpectBucketCount(
        "BackgroundSync.Registration.OneShot.NumAttemptsForSuccessfulEvent", 1,
        0);
  }
}

TEST_F(BackgroundSyncManagerTest, MaxSyncAttemptsWithNotificationPermission) {
  const int max_attempts = 5;
  SetMaxSyncAttemptsAndRestartManager(max_attempts);
  MockPermissionManager* mock_permission_manager =
      GetPermissionControllerDelegate();

  {
    EXPECT_TRUE(Register(sync_options_1_));
    EXPECT_EQ(callback_one_shot_sync_registration_->max_attempts(),
              max_attempts);
  }

  {
    ON_CALL(*mock_permission_manager,
            GetPermissionStatus(PermissionType::NOTIFICATIONS, _, _))
        .WillByDefault(Return(blink::mojom::PermissionStatus::GRANTED));
    EXPECT_TRUE(Register(sync_options_2_));
    EXPECT_EQ(callback_one_shot_sync_registration_->max_attempts(),
              max_attempts + 1);
  }
}

}  // namespace content
