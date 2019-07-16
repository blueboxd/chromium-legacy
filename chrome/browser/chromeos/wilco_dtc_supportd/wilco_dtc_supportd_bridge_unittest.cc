// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_bridge.h"
#include "chrome/services/wilco_dtc_supportd/public/mojom/wilco_dtc_supportd.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_wilco_dtc_supportd_client.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/handle.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace chromeos {

namespace {

class MockMojoWilcoDtcSupportdService
    : public wilco_dtc_supportd::mojom::WilcoDtcSupportdService {
 public:
  MOCK_METHOD2(SendUiMessageToWilcoDtc,
               void(mojo::ScopedHandle, SendUiMessageToWilcoDtcCallback));
  MOCK_METHOD0(NotifyConfigurationDataChanged, void());
};

// Fake implementation of the WilcoDtcSupportdServiceFactory Mojo interface that
// holds up method calls and allows to complete them afterwards.
class FakeMojoWilcoDtcSupportdServiceFactory final
    : public wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory {
 public:
  // WilcoDtcSupportdServiceFactory overrides:

  void GetService(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceRequest service,
      wilco_dtc_supportd::mojom::WilcoDtcSupportdClientPtr client,
      GetServiceCallback callback) override {
    EXPECT_FALSE(pending_get_service_call_);
    pending_get_service_call_ = PendingGetServiceCall{
        std::move(service), std::move(client), std::move(callback)};
  }

  // Completes the Mojo binding of this instance to the given Mojo interface
  // request.
  void Bind(wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactoryRequest
                request) {
    // Close the Mojo binding in case it was previously completed, to allow
    // calling this method multiple times.
    self_binding_.Close();

    self_binding_.Bind(std::move(request));

    self_binding_.set_connection_error_handler(
        base::BindOnce(&FakeMojoWilcoDtcSupportdServiceFactory::OnBindingError,
                       base::Unretained(this)));
  }

  // Closes the Mojo binding of this instance.
  void CloseBinding() {
    self_binding_.Close();

    // Drop the current pending GetService call, if there was any, to allow our
    // instance to be used for new GetService calls after this instance gets
    // bound again.
    pending_get_service_call_.reset();
  }

  // Whether there's a pending GetService call.
  bool is_get_service_call_in_flight() const {
    return pending_get_service_call_.has_value();
  }

  // Respond to the current pending GetService call.
  wilco_dtc_supportd::mojom::WilcoDtcSupportdClientPtr RespondToGetServiceCall(
      mojo::Binding<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>*
          mojo_wilco_dtc_supportd_service_binding) {
    DCHECK(pending_get_service_call_);
    PendingGetServiceCall pending_call = std::move(*pending_get_service_call_);
    pending_get_service_call_.reset();
    mojo_wilco_dtc_supportd_service_binding->Bind(
        std::move(pending_call.service));
    std::move(pending_call.callback).Run();
    return std::move(pending_call.client);
  }

 private:
  struct PendingGetServiceCall {
    wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceRequest service;
    wilco_dtc_supportd::mojom::WilcoDtcSupportdClientPtr client;
    GetServiceCallback callback;
  };

  void OnBindingError() {
    // Drop the current pending GetService call, if there was any, to allow our
    // instance to be used for new GetService calls after this instance gets
    // bound again.
    pending_get_service_call_.reset();
  }

  mojo::Binding<wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>
      self_binding_{this};

  base::Optional<PendingGetServiceCall> pending_get_service_call_;
};

// Fake implementation of the WilcoDtcSupportdBridge delegate that simulates
// Mojo operations that are impossible in the unit test.
class FakeWilcoDtcSupportdBridgeDelegate final
    : public WilcoDtcSupportdBridge::Delegate {
 public:
  explicit FakeWilcoDtcSupportdBridgeDelegate(
      FakeMojoWilcoDtcSupportdServiceFactory*
          mojo_wilco_dtc_supportd_service_factory)
      : mojo_wilco_dtc_supportd_service_factory_(
            mojo_wilco_dtc_supportd_service_factory) {}

  void CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactoryPtr*
          wilco_dtc_supportd_service_factory_mojo_ptr,
      base::ScopedFD* remote_endpoint_fd) override {
    // Bind the Mojo pointer passed to the bridge with the
    // FakeMojoWilcoDtcSupportdServiceFactory implementation.
    mojo_wilco_dtc_supportd_service_factory_->Bind(
        mojo::MakeRequest(wilco_dtc_supportd_service_factory_mojo_ptr));

    // Return a fake file descriptor - its value is not used in the unit test
    // environment for anything except comparing with zero.
    remote_endpoint_fd->reset(HANDLE_EINTR(dup(STDIN_FILENO)));
    DCHECK(remote_endpoint_fd->is_valid());
  }

 private:
  FakeMojoWilcoDtcSupportdServiceFactory* const
      mojo_wilco_dtc_supportd_service_factory_;
};

class MockWilcoDtcSupportdNotificationController
    : public WilcoDtcSupportdNotificationController {
 public:
  explicit MockWilcoDtcSupportdNotificationController(
      ProfileManager* profile_manager)
      : WilcoDtcSupportdNotificationController(profile_manager) {}
  MOCK_CONST_METHOD0(ShowBatteryAuthNotification, std::string());
  MOCK_CONST_METHOD0(ShowNonWilcoChargerNotification, std::string());
  MOCK_CONST_METHOD0(ShowIncompatibleDockNotification, std::string());
  MOCK_CONST_METHOD0(ShowDockErrorNotification, std::string());
};

// Tests for the WilcoDtcSupportdBridge class.
class WilcoDtcSupportdBridgeTest : public testing::Test {
 protected:
  WilcoDtcSupportdBridgeTest() {
    DBusThreadManager::Initialize();
    CHECK(DBusThreadManager::Get()->IsUsingFakes());

    auto profile_manager = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    CHECK(profile_manager->SetUp());
    auto notification_controller = std::make_unique<
        StrictMock<MockWilcoDtcSupportdNotificationController>>(
        profile_manager->profile_manager());

    // Hold a reference to MockWilcoDtcSupportdNotificationController to set
    // expectations on tests before transferring ownership to
    // WilcoDtcSupportdBridge.
    notification_controller_ = notification_controller.get();

    wilco_dtc_supportd_bridge_ = std::make_unique<WilcoDtcSupportdBridge>(
        std::make_unique<FakeWilcoDtcSupportdBridgeDelegate>(
            &mojo_wilco_dtc_supportd_service_factory_),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        std::move(notification_controller));
  }

  ~WilcoDtcSupportdBridgeTest() override {
    wilco_dtc_supportd_bridge_.reset();
    DBusThreadManager::Shutdown();
  }

  StrictMock<MockWilcoDtcSupportdNotificationController>*
  notification_controller() {
    return notification_controller_;
  }

  WilcoDtcSupportdBridge* wilco_dtc_supportd_bridge() {
    return wilco_dtc_supportd_bridge_.get();
  }

  FakeWilcoDtcSupportdClient* wilco_dtc_supportd_dbus_client() {
    WilcoDtcSupportdClient* const wilco_dtc_supportd_client =
        DBusThreadManager::Get()->GetWilcoDtcSupportdClient();
    DCHECK(wilco_dtc_supportd_client);
    return static_cast<FakeWilcoDtcSupportdClient*>(wilco_dtc_supportd_client);
  }

  // Whether there's a pending GetService Mojo call held up by the fake.
  bool is_mojo_factory_get_service_call_in_flight() const {
    return mojo_wilco_dtc_supportd_service_factory_
        .is_get_service_call_in_flight();
  }

  // Reply to the pending GetService Mojo call held up by the fake.
  void RespondToMojoFactoryGetServiceCall() {
    // Close the binding, if it was completed, to allow calling this method
    // multiple times.
    mojo_wilco_dtc_supportd_service_binding_.Close();

    mojo_wilco_dtc_supportd_client_ =
        mojo_wilco_dtc_supportd_service_factory_.RespondToGetServiceCall(
            &mojo_wilco_dtc_supportd_service_binding_);
  }

  // Simulates Mojo connection error.
  void AbortMojoConnection() {
    mojo_wilco_dtc_supportd_service_factory_.CloseBinding();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME};

 private:
  FakeMojoWilcoDtcSupportdServiceFactory
      mojo_wilco_dtc_supportd_service_factory_;

  MockMojoWilcoDtcSupportdService mojo_wilco_dtc_supportd_service_;
  mojo::Binding<wilco_dtc_supportd::mojom::WilcoDtcSupportdService>
      mojo_wilco_dtc_supportd_service_binding_{
          &mojo_wilco_dtc_supportd_service_};

  StrictMock<MockWilcoDtcSupportdNotificationController>*
      notification_controller_;

  std::unique_ptr<WilcoDtcSupportdBridge> wilco_dtc_supportd_bridge_;

  wilco_dtc_supportd::mojom::WilcoDtcSupportdClientPtr
      mojo_wilco_dtc_supportd_client_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

}  // namespace

// Test successful Mojo bootstrapping scenario.
TEST_F(WilcoDtcSupportdBridgeTest, SuccessfulBootstrap) {
  // Initially the bridge is blocked on the WaitForServiceToBeAvailable call.
  EXPECT_EQ(1, wilco_dtc_supportd_dbus_client()
                   ->wait_for_service_to_be_available_in_flight_call_count());
  EXPECT_FALSE(wilco_dtc_supportd_dbus_client()
                   ->bootstrap_mojo_connection_in_flight_call_count());

  // Resolve the pending WaitForServiceToBeAvailable call. Verify the bridge
  // makes the BootstrapMojoConnection D-Bus call.
  wilco_dtc_supportd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);
  EXPECT_EQ(1, wilco_dtc_supportd_dbus_client()
                   ->bootstrap_mojo_connection_in_flight_call_count());

  // Resolve the pending BootstrapMojoConnection D-Bus call (but then revert the
  // fake in order to hold up its subsequent calls). Verify the bridge makes the
  // GetService Mojo call on the WilcoDtcSupportdServiceFactory interface.
  wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(
      base::nullopt);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());

  // Resolve the pending GetService Mojo call. Verify the bridge exposes the
  // obtained WilcoDtcSupportdService Mojo interface pointer.
  RespondToMojoFactoryGetServiceCall();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(
      wilco_dtc_supportd_bridge()->wilco_dtc_supportd_service_mojo_proxy());

  // Verify that no extra D-Bus or Mojo calls are made.
  EXPECT_FALSE(wilco_dtc_supportd_dbus_client()
                   ->bootstrap_mojo_connection_in_flight_call_count());
  EXPECT_FALSE(is_mojo_factory_get_service_call_in_flight());
}

// Test the case when the D-Bus service is permanently unavailable - the bridge
// should be just blocked on a single WaitForServiceToBeAvailable call.
TEST_F(WilcoDtcSupportdBridgeTest, DBusServiceNotBringingUpError) {
  EXPECT_EQ(1, wilco_dtc_supportd_dbus_client()
                   ->wait_for_service_to_be_available_in_flight_call_count());

  // Verify that no extra WaitForServiceToBeAvailable calls are made.
  scoped_task_environment_.FastForwardBy(
      WilcoDtcSupportdBridge::connection_attempt_interval_for_testing());
  EXPECT_EQ(1, wilco_dtc_supportd_dbus_client()
                   ->wait_for_service_to_be_available_in_flight_call_count());
}

// Test the case when the BootstrapMojoConnection D-Bus method fails
// permanently - the bridge should give up making attempts after a few retries.
TEST_F(WilcoDtcSupportdBridgeTest, DBusBootstrapMojoConnectionError) {
  wilco_dtc_supportd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);

  for (int attempt_number = 0;
       attempt_number <
       WilcoDtcSupportdBridge::max_connection_attempt_count_for_testing();
       ++attempt_number) {
    // Fail the pending BootstrapMojoConnection call, but then revert the fake
    // in order to hold up its subsequent calls.
    EXPECT_EQ(1, wilco_dtc_supportd_dbus_client()
                     ->bootstrap_mojo_connection_in_flight_call_count());
    wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(false);
    wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(
        base::nullopt);
    scoped_task_environment_.RunUntilIdle();

    // Verify that no new BootstrapMojoConnection call is made immediately after
    // the previous one failed.
    EXPECT_FALSE(wilco_dtc_supportd_dbus_client()
                     ->bootstrap_mojo_connection_in_flight_call_count());

    // Fast forward the clock till the next attempt should occur.
    scoped_task_environment_.FastForwardBy(
        WilcoDtcSupportdBridge::connection_attempt_interval_for_testing());
  }

  // No new BootstrapMojoConnection calls are made after the retry limit
  // exceeded.
  EXPECT_FALSE(wilco_dtc_supportd_dbus_client()
                   ->bootstrap_mojo_connection_in_flight_call_count());
}

// Test the case when the Mojo connection gets aborted before the first Mojo
// call (GetService on the WilcoDtcSupportdServiceFactory interface) completes -
// the bridge should give up making attempts after a few retries.
TEST_F(WilcoDtcSupportdBridgeTest, ImmediateMojoDisconnectionError) {
  wilco_dtc_supportd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);
  wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  scoped_task_environment_.RunUntilIdle();

  for (int attempt_number = 0;
       attempt_number <
       WilcoDtcSupportdBridge::max_connection_attempt_count_for_testing();
       ++attempt_number) {
    // Verify that the bridge made the GetService Mojo call (on the
    // WilcoDtcSupportdServiceFactory interface). Abort the Mojo binding without
    // responding to the call. Verify that no new call happens immediately.
    EXPECT_TRUE(is_mojo_factory_get_service_call_in_flight());
    AbortMojoConnection();
    scoped_task_environment_.RunUntilIdle();
    EXPECT_FALSE(is_mojo_factory_get_service_call_in_flight());

    // Fast forward the clock till the next attempt should occur.
    scoped_task_environment_.FastForwardBy(
        WilcoDtcSupportdBridge::connection_attempt_interval_for_testing());
  }

  // No new connection attempts are made after the retry limit exceeded.
  EXPECT_FALSE(is_mojo_factory_get_service_call_in_flight());
}

// Test that the Mojo connection gets bootstrapped again after the previous one
// got aborted.
TEST_F(WilcoDtcSupportdBridgeTest, Reestablishing) {
  // Let the bootstrapping succeed on the first attempt.
  wilco_dtc_supportd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);
  wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());
  RespondToMojoFactoryGetServiceCall();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(
      wilco_dtc_supportd_bridge()->wilco_dtc_supportd_service_mojo_proxy());

  // Abort the Mojo binding. Verify that no new connection attempt happens
  // immediately.
  AbortMojoConnection();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(
      wilco_dtc_supportd_bridge()->wilco_dtc_supportd_service_mojo_proxy());
  EXPECT_FALSE(is_mojo_factory_get_service_call_in_flight());

  // Fast forward the clock till the next connection attempt.
  scoped_task_environment_.FastForwardBy(
      WilcoDtcSupportdBridge::connection_attempt_interval_for_testing());

  // Let the bootstrapping succeed again.
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());
  RespondToMojoFactoryGetServiceCall();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(
      wilco_dtc_supportd_bridge()->wilco_dtc_supportd_service_mojo_proxy());
}

// Test that the bridge resets its retry counter after a successful
// bootstrapping takes place.
TEST_F(WilcoDtcSupportdBridgeTest, RetryCounterReset) {
  wilco_dtc_supportd_dbus_client()->SetWaitForServiceToBeAvailableResult(true);

  // Fail the first few connection attempts, leaving only one attempt left.
  wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(false);
  scoped_task_environment_.FastForwardBy(
      WilcoDtcSupportdBridge::connection_attempt_interval_for_testing() *
      (WilcoDtcSupportdBridge::max_connection_attempt_count_for_testing() - 2));

  // Let the bootstrapping succeed on the new attempt (the last allowed one in
  // this serie).
  wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  scoped_task_environment_.FastForwardBy(
      WilcoDtcSupportdBridge::connection_attempt_interval_for_testing());
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());
  RespondToMojoFactoryGetServiceCall();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(
      wilco_dtc_supportd_bridge()->wilco_dtc_supportd_service_mojo_proxy());

  // Abort the Mojo binding.
  AbortMojoConnection();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(
      wilco_dtc_supportd_bridge()->wilco_dtc_supportd_service_mojo_proxy());

  // Fail again a few attempts as before.
  wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(false);
  scoped_task_environment_.FastForwardBy(
      WilcoDtcSupportdBridge::connection_attempt_interval_for_testing() *
      (WilcoDtcSupportdBridge::max_connection_attempt_count_for_testing() - 1));

  // Let the bootstrapping succeed again as before. Note that this verifies that
  // the retry attempts made before the previous successful bootstrap were
  // ignored.
  wilco_dtc_supportd_dbus_client()->SetBootstrapMojoConnectionResult(true);
  scoped_task_environment_.FastForwardBy(
      WilcoDtcSupportdBridge::connection_attempt_interval_for_testing());
  ASSERT_TRUE(is_mojo_factory_get_service_call_in_flight());
  RespondToMojoFactoryGetServiceCall();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(
      wilco_dtc_supportd_bridge()->wilco_dtc_supportd_service_mojo_proxy());
}

// Test that the bridge calls the right method on the notification controller.
TEST_F(WilcoDtcSupportdBridgeTest, HandleEvent) {
  EXPECT_CALL(*notification_controller(), ShowBatteryAuthNotification());
  wilco_dtc_supportd_bridge()->HandleEvent(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent::kBatteryAuth);

  EXPECT_CALL(*notification_controller(), ShowIncompatibleDockNotification());
  wilco_dtc_supportd_bridge()->HandleEvent(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent::kIncompatibleDock);

  EXPECT_CALL(*notification_controller(), ShowNonWilcoChargerNotification());
  wilco_dtc_supportd_bridge()->HandleEvent(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent::kNonWilcoCharger);

  EXPECT_CALL(*notification_controller(), ShowDockErrorNotification());
  wilco_dtc_supportd_bridge()->HandleEvent(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent::kDockError);
}

}  // namespace chromeos
