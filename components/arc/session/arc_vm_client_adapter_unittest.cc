// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_debug_daemon_client.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr const char kUserIdHash[] = "this_is_a_valid_user_id_hash";

// A debugd client that can fail to start Concierge.
// TODO(yusukes): Merge the feature to FakeDebugDaemonClient.
class TestDebugDaemonClient : public chromeos::FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  ~TestDebugDaemonClient() override = default;

  void StartConcierge(ConciergeCallback callback) override {
    start_concierge_called_ = true;
    std::move(callback).Run(start_concierge_result_);
  }

  bool start_concierge_called() const { return start_concierge_called_; }
  void set_start_concierge_result(bool result) {
    start_concierge_result_ = result;
  }

 private:
  bool start_concierge_called_ = false;
  bool start_concierge_result_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestDebugDaemonClient);
};

// A concierge that remembers the parameter passed to StartArcVm.
// TODO(yusukes): Merge the feature to FakeConciergeClient.
class TestConciergeClient : public chromeos::FakeConciergeClient {
 public:
  TestConciergeClient() = default;
  ~TestConciergeClient() override = default;

  void StartArcVm(
      const vm_tools::concierge::StartArcVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) override {
    start_arc_vm_request_ = request;
    chromeos::FakeConciergeClient::StartArcVm(request, std::move(callback));
  }

  const vm_tools::concierge::StartArcVmRequest& start_arc_vm_request() const {
    return start_arc_vm_request_;
  }

 private:
  vm_tools::concierge::StartArcVmRequest start_arc_vm_request_;

  DISALLOW_COPY_AND_ASSIGN(TestConciergeClient);
};

class ArcVmClientAdapterTest : public testing::Test,
                               public ArcClientAdapter::Observer {
 public:
  ArcVmClientAdapterTest() {
    // Create and set new fake clients every time to reset clients' status.
    chromeos::DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
        std::make_unique<TestDebugDaemonClient>());
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        std::make_unique<TestConciergeClient>());
  }

  ~ArcVmClientAdapterTest() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        nullptr);
    chromeos::DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
        nullptr);
  }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    adapter_ = CreateArcVmClientAdapter();
    arc_instance_stopped_called_ = false;
    adapter_->AddObserver(this);

    // The fake client returns VM_STATUS_STARTING by default. Change it
    // to VM_STATUS_RUNNING which is used by ARCVM.
    vm_tools::concierge::StartVmResponse start_vm_response;
    start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
    GetTestConciergeClient()->set_start_vm_response(start_vm_response);
  }

  void TearDown() override {
    adapter_->RemoveObserver(this);
    adapter_.reset();
    run_loop_.reset();
  }

  // ArcClientAdapter::Observer:
  void ArcInstanceStopped() override {
    arc_instance_stopped_called_ = true;
    run_loop()->Quit();
  }

  void ExpectTrueThenQuit(bool result) {
    EXPECT_TRUE(result);
    run_loop()->Quit();
  }

  void ExpectFalseThenQuit(bool result) {
    EXPECT_FALSE(result);
    run_loop()->Quit();
  }

 protected:
  bool GetStartConciergeCalled() {
    return GetTestDebugDaemonClient()->start_concierge_called();
  }

  void SetStartConciergeResponse(bool response) {
    GetTestDebugDaemonClient()->set_start_concierge_result(response);
  }

  void SetValidUserIdHash() { adapter()->SetUserIdHashForProfile(kUserIdHash); }

  void RestartRunLoop() { run_loop_ = std::make_unique<base::RunLoop>(); }

  base::RunLoop* run_loop() { return run_loop_.get(); }
  ArcClientAdapter* adapter() { return adapter_.get(); }
  bool arc_instance_stopped_called() const {
    return arc_instance_stopped_called_;
  }
  TestConciergeClient* GetTestConciergeClient() {
    return static_cast<TestConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }

 private:
  TestDebugDaemonClient* GetTestDebugDaemonClient() {
    return static_cast<TestDebugDaemonClient*>(
        chromeos::DBusThreadManager::Get()->GetDebugDaemonClient());
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ArcClientAdapter> adapter_;
  bool arc_instance_stopped_called_;

  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapterTest);
};

// Tests that SetUserIdHashForProfile() doesn't crash.
TEST_F(ArcVmClientAdapterTest, SetUserIdHashForProfile) {
  adapter()->SetUserIdHashForProfile("deadbeef");
}

// Tests that StartMiniArc() always succeeds.
TEST_F(ArcVmClientAdapterTest, StartMiniArc) {
  StartArcMiniContainerRequest req;
  adapter()->StartMiniArc(
      req, base::BindOnce(&ArcVmClientAdapterTest::ExpectTrueThenQuit,
                          base::Unretained(this)));
  run_loop()->Run();
  // Also confirm that no VM is started. ARCVM doesn't support mini ARC.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StopArcInstance() notifies the observer.
TEST_F(ArcVmClientAdapterTest, StopArcInstance) {
  adapter()->StopArcInstance();
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that UpgradeArc() handles StartConcierge() failures properly. This test
// inherits ArcVmClientAdapterTest because it uses its own DebugDaemonClient
// instead of the default one for testing.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartConciergeFailure) {
  SetValidUserIdHash();
  // Inject failure to StartConcierge().
  SetStartConciergeResponse(false);
  UpgradeArcContainerRequest req;
  adapter()->UpgradeArc(
      req, base::BindOnce(&ArcVmClientAdapterTest::ExpectFalseThenQuit,
                          base::Unretained(this)));
  run_loop()->Run();
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM.
  RestartRunLoop();
  adapter()->StopArcInstance();
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that "no user ID hash" failure is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_NoUserId) {
  // Don't call SetValidUserIdHash(). Note that we cannot call StartArcVm()
  // without a valid ID.
  UpgradeArcContainerRequest req;
  adapter()->UpgradeArc(
      req, base::BindOnce(&ArcVmClientAdapterTest::ExpectFalseThenQuit,
                          base::Unretained(this)));
  run_loop()->Run();
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM.
  RestartRunLoop();
  adapter()->StopArcInstance();
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that StartArcVm() failure is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmFailure) {
  SetValidUserIdHash();
  // Inject failure to StartArcVm().
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_UNKNOWN);
  GetTestConciergeClient()->set_start_vm_response(start_vm_response);

  UpgradeArcContainerRequest req;
  adapter()->UpgradeArc(
      req, base::BindOnce(&ArcVmClientAdapterTest::ExpectFalseThenQuit,
                          base::Unretained(this)));
  run_loop()->Run();
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM.
  RestartRunLoop();
  adapter()->StopArcInstance();
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that successful StartArcVm() call is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_Success) {
  SetValidUserIdHash();
  UpgradeArcContainerRequest req;
  adapter()->UpgradeArc(
      req, base::BindOnce(&ArcVmClientAdapterTest::ExpectTrueThenQuit,
                          base::Unretained(this)));
  run_loop()->Run();
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM.
  RestartRunLoop();
  adapter()->StopArcInstance();
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that StartArcVm() is called with valid parameters.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmParams) {
  SetValidUserIdHash();
  UpgradeArcContainerRequest req;
  adapter()->UpgradeArc(
      req, base::BindOnce(&ArcVmClientAdapterTest::ExpectTrueThenQuit,
                          base::Unretained(this)));
  run_loop()->Run();
  ASSERT_TRUE(GetTestConciergeClient()->start_arc_vm_called());

  // Verify parameters
  const auto& params = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ("arcvm", params.name());
  EXPECT_EQ(kUserIdHash, params.owner_id());
  EXPECT_FALSE(params.vm().kernel().empty());
  // Make sure system.raw.img is passed.
  EXPECT_FALSE(params.vm().rootfs().empty());
  // Make sure vendor.raw.img is passed.
  EXPECT_LE(1, params.disks_size());
  EXPECT_LT(0, params.params_size());
}

}  // namespace
}  // namespace arc
