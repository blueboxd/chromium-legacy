// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"

#include <unistd.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/common/mac/app_shim_param_traits.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestingAppShim : public chrome::mojom::AppShim {
 public:
  TestingAppShim() {}

  chrome::mojom::AppShimHostBootstrap::LaunchAppCallback
  GetLaunchAppCallback() {
    return base::BindOnce(&TestingAppShim::LaunchAppDone,
                          base::Unretained(this));
  }
  chrome::mojom::AppShimHostBootstrapRequest GetHostBootstrapRequest() {
    return mojo::MakeRequest(&host_bootstrap_ptr_);
  }

  apps::AppShimLaunchResult GetLaunchResult() const {
    EXPECT_TRUE(received_launch_done_result_);
    return launch_done_result_;
  }

 private:
  void LaunchAppDone(apps::AppShimLaunchResult result,
                     chrome::mojom::AppShimRequest app_shim_request) {
    received_launch_done_result_ = true;
    launch_done_result_ = result;
  }

  // chrome::mojom::AppShim implementation.
  void CreateRemoteCocoaApplication(
      remote_cocoa::mojom::ApplicationAssociatedRequest request) override {}
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override {}
  void Hide() override {}
  void UnhideWithoutActivation() override {}
  void SetUserAttention(apps::AppShimAttentionType attention_type) override {}
  void SetBadgeLabel(const std::string& badge_label) override {}

  bool received_launch_done_result_ = false;
  apps::AppShimLaunchResult launch_done_result_ = apps::APP_SHIM_LAUNCH_SUCCESS;

  chrome::mojom::AppShimHostBootstrapPtr host_bootstrap_ptr_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShim);
};

class TestingAppShimHost : public AppShimHost {
 public:
  TestingAppShimHost(const std::string& app_id,
                     const base::FilePath& profile_path)
      : AppShimHost(app_id, profile_path, false /* uses_remote_views */),
        test_weak_factory_(this) {}

  base::WeakPtr<TestingAppShimHost> GetWeakPtr() {
    return test_weak_factory_.GetWeakPtr();
  }

 private:
  ~TestingAppShimHost() override {}
  base::WeakPtrFactory<TestingAppShimHost> test_weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimHost);
};

class TestingAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  explicit TestingAppShimHostBootstrap(
      chrome::mojom::AppShimHostBootstrapRequest host_request)
      : AppShimHostBootstrap(getpid()), test_weak_factory_(this) {
    // AppShimHost will bind to the request from ServeChannel. For testing
    // purposes, have this request passed in at creation.
    host_bootstrap_binding_.Bind(std::move(host_request));
  }

  base::WeakPtr<TestingAppShimHostBootstrap> GetWeakPtr() {
    return test_weak_factory_.GetWeakPtr();
  }

  using AppShimHostBootstrap::LaunchApp;

 private:
  base::WeakPtrFactory<TestingAppShimHostBootstrap> test_weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimHostBootstrap);
};

const char kTestAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestProfileDir[] = "Profile 1";

class AppShimHostTest : public testing::Test,
                        public apps::AppShimHandler {
 public:
  AppShimHostTest() { task_runner_ = base::ThreadTaskRunnerHandle::Get(); }

  ~AppShimHostTest() override {
    if (host_)
      host_->OnAppClosed();
    DCHECK(!host_);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }
  AppShimHost* host() { return host_.get(); }
  chrome::mojom::AppShimHost* GetMojoHost() { return host_ptr_.get(); }

  void LaunchApp(apps::AppShimLaunchType launch_type) {
    // Ownership of TestingAppShimHostBootstrap will be transferred to its host.
    (new TestingAppShimHostBootstrap(shim_->GetHostBootstrapRequest()))
        ->LaunchApp(mojo::MakeRequest(&host_ptr_),
                    base::FilePath(kTestProfileDir), kTestAppId, launch_type,
                    std::vector<base::FilePath>(),
                    shim_->GetLaunchAppCallback());
  }

  apps::AppShimLaunchResult GetLaunchResult() {
    RunUntilIdle();
    return shim_->GetLaunchResult();
  }

  void SimulateDisconnect() { host_ptr_.reset(); }

 protected:
  void OnShimLaunchRequested(
      AppShimHost* host,
      bool recreate_shims,
      apps::ShimLaunchedCallback launched_callback,
      apps::ShimTerminatedCallback terminated_callback) override {}
  void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override {
    ++launch_count_;
    if (bootstrap->GetLaunchType() == apps::APP_SHIM_LAUNCH_NORMAL)
      ++launch_now_count_;
    // Maintain only a weak reference to |host_| because it is owned by itself
    // and will delete itself upon closing.
    host_ = (new TestingAppShimHost(bootstrap->GetAppId(),
                                    bootstrap->GetProfilePath()))
                ->GetWeakPtr();
    if (launch_result_ == apps::APP_SHIM_LAUNCH_SUCCESS)
      host_->OnBootstrapConnected(std::move(bootstrap));
    else
      bootstrap->OnFailedToConnectToHost(launch_result_);
  }

  void OnShimClose(AppShimHost* host) override { ++close_count_; }

  void OnShimFocus(AppShimHost* host,
                   apps::AppShimFocusType focus_type,
                   const std::vector<base::FilePath>& file) override {
    ++focus_count_;
  }

  void OnShimSetHidden(AppShimHost* host, bool hidden) override {}

  void OnShimQuit(AppShimHost* host) override { ++quit_count_; }

  apps::AppShimLaunchResult launch_result_ = apps::APP_SHIM_LAUNCH_SUCCESS;
  int launch_count_ = 0;
  int launch_now_count_ = 0;
  int close_count_ = 0;
  int focus_count_ = 0;
  int quit_count_ = 0;

 private:
  void SetUp() override {
    testing::Test::SetUp();
    shim_.reset(new TestingAppShim());
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestingAppShim> shim_;

  std::unique_ptr<AppShimHostBootstrap> launched_bootstrap_;

  // AppShimHost will destroy itself in AppShimHost::Close, so use a weak
  // pointer here to avoid lifetime issues.
  base::WeakPtr<TestingAppShimHost> host_;
  chrome::mojom::AppShimHostPtr host_ptr_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostTest);
};


}  // namespace

TEST_F(AppShimHostTest, TestLaunchAppWithHandler) {
  apps::AppShimHandler::RegisterHandler(kTestAppId, this);
  LaunchApp(apps::APP_SHIM_LAUNCH_NORMAL);
  EXPECT_EQ(kTestAppId, host()->GetAppId());
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_SUCCESS, GetLaunchResult());
  EXPECT_EQ(1, launch_count_);
  EXPECT_EQ(1, launch_now_count_);
  EXPECT_EQ(0, focus_count_);
  EXPECT_EQ(0, close_count_);

  GetMojoHost()->FocusApp(apps::APP_SHIM_FOCUS_NORMAL,
                          std::vector<base::FilePath>());
  RunUntilIdle();
  EXPECT_EQ(1, focus_count_);

  GetMojoHost()->QuitApp();
  RunUntilIdle();
  EXPECT_EQ(1, quit_count_);

  SimulateDisconnect();
  RunUntilIdle();
  EXPECT_EQ(1, close_count_);
  EXPECT_EQ(nullptr, host());
  apps::AppShimHandler::RemoveHandler(kTestAppId);
}

TEST_F(AppShimHostTest, TestNoLaunchNow) {
  apps::AppShimHandler::RegisterHandler(kTestAppId, this);
  LaunchApp(apps::APP_SHIM_LAUNCH_REGISTER_ONLY);
  EXPECT_EQ(kTestAppId, host()->GetAppId());
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_SUCCESS, GetLaunchResult());
  EXPECT_EQ(1, launch_count_);
  EXPECT_EQ(0, launch_now_count_);
  EXPECT_EQ(0, focus_count_);
  EXPECT_EQ(0, close_count_);
  apps::AppShimHandler::RemoveHandler(kTestAppId);
}

TEST_F(AppShimHostTest, TestFailLaunch) {
  apps::AppShimHandler::RegisterHandler(kTestAppId, this);
  launch_result_ = apps::APP_SHIM_LAUNCH_APP_NOT_FOUND;
  LaunchApp(apps::APP_SHIM_LAUNCH_NORMAL);
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_APP_NOT_FOUND, GetLaunchResult());
  apps::AppShimHandler::RemoveHandler(kTestAppId);
}
