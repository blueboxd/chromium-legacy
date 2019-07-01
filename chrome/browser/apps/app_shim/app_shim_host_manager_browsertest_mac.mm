// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_manager_mac.h"

#include <unistd.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/app_shim/app_shim_controller.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/test/app_shim_host_manager_test_api_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/common/mac/app_shim_param_traits.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/version_info/version_info.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

using LaunchAppCallback =
    chrome::mojom::AppShimHostBootstrap::LaunchAppCallback;

const char kTestAppMode[] = "test_app";

// A test version of the AppShimController mojo client in chrome_main_app_mode.
class TestShimClient : public chrome::mojom::AppShim {
 public:
  TestShimClient();

  // Friend accessor.
  mojo::PlatformChannelEndpoint ConnectToBrowser(
      const mojo::NamedPlatformChannel::ServerName& server_name) {
    return AppShimController::ConnectToBrowser(server_name);
  }

  chrome::mojom::AppShimHostRequest GetHostRequest() {
    return std::move(host_request_);
  }
  LaunchAppCallback GetLaunchAppCallback() {
    return base::BindOnce(&TestShimClient::LaunchAppDone,
                          base::Unretained(this));
  }

  chrome::mojom::AppShimHostPtr& host() { return host_; }
  chrome::mojom::AppShimHostBootstrapPtr& host_bootstrap() {
    return host_bootstrap_;
  }

  // chrome::mojom::AppShim implementation (not used in testing, but can be).
  void CreateRemoteCocoaApplication(
      remote_cocoa::mojom::ApplicationAssociatedRequest request) override {}
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override {}
  void Hide() override {}
  void UnhideWithoutActivation() override {}
  void SetUserAttention(apps::AppShimAttentionType attention_type) override {}
  void SetBadgeLabel(const std::string& badge_label) override {}

 private:
  void LaunchAppDone(apps::AppShimLaunchResult result,
                     chrome::mojom::AppShimRequest app_shim_request) {
    shim_binding_.Bind(std::move(app_shim_request));
  }

  mojo::IsolatedConnection mojo_connection_;
  mojo::Binding<chrome::mojom::AppShim> shim_binding_;
  chrome::mojom::AppShimHostPtr host_;
  chrome::mojom::AppShimHostRequest host_request_;
  chrome::mojom::AppShimHostBootstrapPtr host_bootstrap_;

  DISALLOW_COPY_AND_ASSIGN(TestShimClient);
};

TestShimClient::TestShimClient()
    : shim_binding_(this), host_request_(mojo::MakeRequest(&host_)) {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));

  std::string name_fragment =
      base::StringPrintf("%s.%s.%s", base::mac::BaseBundleID(),
                         app_mode::kAppShimBootstrapNameFragment,
                         base::MD5String(user_data_dir.value()).c_str());
  mojo::PlatformChannelEndpoint endpoint = ConnectToBrowser(name_fragment);

  mojo::ScopedMessagePipeHandle message_pipe =
      mojo_connection_.Connect(std::move(endpoint));
  host_bootstrap_ = chrome::mojom::AppShimHostBootstrapPtr(
      chrome::mojom::AppShimHostBootstrapPtrInfo(std::move(message_pipe), 0));
}

// Browser Test for AppShimHostManager to test IPC interactions.
class AppShimHostManagerBrowserTest : public InProcessBrowserTest,
                                      public apps::AppShimHandler,
                                      public chrome::mojom::AppShimHost {
 public:
  AppShimHostManagerBrowserTest() : binding_(this) {}

 protected:
  // Wait for OnShimProcessConnected, then send a quit, and wait for the
  // response. Used to test launch behavior.
  void RunAndExitGracefully();

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // AppShimHandler overrides:
  void OnShimLaunchRequested(
      ::AppShimHost* host,
      bool recreate_shims,
      apps::ShimLaunchedCallback launched_callback,
      apps::ShimTerminatedCallback terminated_callback) override {}
  void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override;
  void OnShimClose(::AppShimHost* host) override {}
  void OnShimFocus(::AppShimHost* host,
                   apps::AppShimFocusType focus_type,
                   const std::vector<base::FilePath>& files) override {}
  void OnShimSetHidden(::AppShimHost* host, bool hidden) override {}
  void OnShimQuit(::AppShimHost* host) override {}

  std::unique_ptr<TestShimClient> test_client_;
  std::vector<base::FilePath> last_launch_files_;
  apps::AppShimLaunchType last_launch_type_ = apps::APP_SHIM_LAUNCH_NUM_TYPES;

 private:
  // chrome::mojom::AppShimHost.
  void FocusApp(apps::AppShimFocusType focus_type,
                const std::vector<base::FilePath>& files) override {}
  void SetAppHidden(bool hidden) override {}
  void QuitApp() override {
    ++quit_count_;
    runner_->Quit();
  }

  std::unique_ptr<base::RunLoop> runner_;
  mojo::Binding<chrome::mojom::AppShimHost> binding_;
  chrome::mojom::AppShimPtr app_shim_ptr_;

  int launch_count_ = 0;
  int quit_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManagerBrowserTest);
};

void AppShimHostManagerBrowserTest::RunAndExitGracefully() {
  runner_ = std::make_unique<base::RunLoop>();
  EXPECT_EQ(0, launch_count_);
  runner_->Run();  // Will stop in OnShimProcessConnected().
  EXPECT_EQ(1, launch_count_);

  runner_ = std::make_unique<base::RunLoop>();
  test_client_->host()->QuitApp();
  EXPECT_EQ(0, quit_count_);
  runner_->Run();  // Will stop in QuitApp().
  EXPECT_EQ(1, quit_count_);

  test_client_.reset();
}

void AppShimHostManagerBrowserTest::SetUpOnMainThread() {
  // Can't do this in the constructor, it needs a BrowserProcess.
  apps::AppShimHandler::RegisterHandler(kTestAppMode, this);
}

void AppShimHostManagerBrowserTest::TearDownOnMainThread() {
  apps::AppShimHandler::RemoveHandler(kTestAppMode);
}

void AppShimHostManagerBrowserTest::OnShimProcessConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  ++launch_count_;
  binding_.Bind(bootstrap->GetLaunchAppShimHostRequest());
  last_launch_type_ = bootstrap->GetLaunchType();
  last_launch_files_ = bootstrap->GetLaunchFiles();

  bootstrap->OnConnectedToHost(mojo::MakeRequest(&app_shim_ptr_));
  runner_->Quit();
}

// Test regular launch, which would ask Chrome to launch the app.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest, LaunchNormal) {
  test_client_.reset(new TestShimClient());
  test_client_->host_bootstrap()->LaunchApp(
      test_client_->GetHostRequest(), browser()->profile()->GetPath(),
      kTestAppMode, apps::APP_SHIM_LAUNCH_NORMAL, std::vector<base::FilePath>(),
      test_client_->GetLaunchAppCallback());

  RunAndExitGracefully();
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_NORMAL, last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Test register-only launch, used when Chrome has already launched the app.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest, LaunchRegisterOnly) {
  test_client_.reset(new TestShimClient());
  test_client_->host_bootstrap()->LaunchApp(
      test_client_->GetHostRequest(), browser()->profile()->GetPath(),
      kTestAppMode, apps::APP_SHIM_LAUNCH_REGISTER_ONLY,
      std::vector<base::FilePath>(), test_client_->GetLaunchAppCallback());

  RunAndExitGracefully();
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_REGISTER_ONLY, last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Ensure bootstrap name registers.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest,
                       PRE_ReCreate) {
  test::AppShimHostManagerTestApi test_api(
      g_browser_process->platform_part()->app_shim_host_manager());
  EXPECT_TRUE(test_api.mach_acceptor());
}

// Ensure the bootstrap name can be re-created after a prior browser process has
// quit.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest,
                       ReCreate) {
  test::AppShimHostManagerTestApi test_api(
      g_browser_process->platform_part()->app_shim_host_manager());
  EXPECT_TRUE(test_api.mach_acceptor());
}

// Tests for the files created by AppShimHostManager.
class AppShimHostManagerBrowserTestSymlink
    : public AppShimHostManagerBrowserTest {
 public:
  AppShimHostManagerBrowserTestSymlink() {}

 protected:
  base::FilePath version_path_;

 private:
  bool SetUpUserDataDirectory() override;
  void TearDownInProcessBrowserTestFixture() override;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManagerBrowserTestSymlink);
};

bool AppShimHostManagerBrowserTestSymlink::SetUpUserDataDirectory() {
  // Create an existing symlink. It should be replaced by AppShimHostManager.
  base::FilePath user_data_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));

  // Create an invalid RunningChromeVersion file.
  version_path_ =
      user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);
  EXPECT_TRUE(base::CreateSymbolicLink(base::FilePath("invalid_version"),
                                       version_path_));
  return AppShimHostManagerBrowserTest::SetUpUserDataDirectory();
}

void AppShimHostManagerBrowserTestSymlink::
    TearDownInProcessBrowserTestFixture() {
  // Check that created files have been deleted.
  EXPECT_FALSE(base::PathExists(version_path_));
}

IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTestSymlink,
                       RunningChromeVersionCorrectlyWritten) {
  // Check that the RunningChromeVersion file is correctly written.
  base::FilePath version;
  EXPECT_TRUE(base::ReadSymbolicLink(version_path_, &version));
  EXPECT_EQ(version_info::GetVersionNumber(), version.value());
}
