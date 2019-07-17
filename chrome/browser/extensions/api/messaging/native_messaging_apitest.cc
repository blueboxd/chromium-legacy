// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/result_catcher.h"

namespace extensions {
namespace {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, NativeMessagingBasic) {
  extensions::ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(false));
  ASSERT_TRUE(RunExtensionTest("native_messaging")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, UserLevelNativeMessaging) {
  extensions::ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(true));
  ASSERT_TRUE(RunExtensionTest("native_messaging")) << message_;
}

#if !defined(OS_CHROMEOS)

class TestProcessManagerObserver : public ProcessManagerObserver {
 public:
  TestProcessManagerObserver() : observer_(this) {}

  void WaitForProcessShutdown(ProcessManager* process_manager,
                              const std::string& extension_id) {
    DCHECK(!quit_);
    extension_id_ = extension_id;
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();

    observer_.Add(process_manager);
    run_loop.Run();
  }

 private:
  void OnBackgroundHostClose(const std::string& extension_id) override {
    if (extension_id != extension_id_) {
      return;
    }
    observer_.RemoveAll();
    extension_id_.clear();
    std::move(quit_).Run();
  }

  std::string extension_id_;
  ScopedObserver<ProcessManager, TestProcessManagerObserver> observer_;
  base::OnceClosure quit_;

  DISALLOW_COPY_AND_ASSIGN(TestProcessManagerObserver);
};

// Disabled on Windows due to timeouts; see https://crbug.com/984897.
#if defined(OS_WIN)
#define MAYBE_NativeMessagingLaunch DISABLED_NativeMessagingLaunch
#else
#define MAYBE_NativeMessagingLaunch NativeMessagingLaunch
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_NativeMessagingLaunch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kOnConnectNative);
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  extensions::ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(false));

  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("native_messaging_launch"));
  TestProcessManagerObserver observer;
  observer.WaitForProcessShutdown(ProcessManager::Get(profile()),
                                  extension->id());

  ResultCatcher catcher;

  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectExtension,
                                 extension->id());
  command_line.AppendSwitchASCII(
      switches::kNativeMessagingConnectHost,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(command_line, {},
                                                          profile()->GetPath());

  if (!catcher.GetNextResult()) {
    FAIL() << catcher.message();
  }
  size_t tabs = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    tabs += browser->tab_strip_model()->count();
  }
  EXPECT_EQ(1u, tabs);
}

// Test that a natively-initiated connection from a host not supporting
// natively-initiated connections is not allowed. The test extension expects the
// channel to be immediately closed with an error.
IN_PROC_BROWSER_TEST_F(
    ExtensionApiTest,
    NativeMessagingLaunch_LaunchFromNativeUnsupportedByNativeHost) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kOnConnectNative);
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  extensions::ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(false));

  auto* extension = LoadExtension(
      test_data_dir_.AppendASCII("native_messaging_launch_unsupported"));
  TestProcessManagerObserver observer;
  observer.WaitForProcessShutdown(ProcessManager::Get(profile()),
                                  extension->id());

  ResultCatcher catcher;

  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectExtension,
                                 extension->id());
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectHost,
                                 ScopedTestNativeMessagingHost::kHostName);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(command_line, {},
                                                          profile()->GetPath());

  if (!catcher.GetNextResult()) {
    FAIL() << catcher.message();
  }
  size_t tabs = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    tabs += browser->tab_strip_model()->count();
  }
  EXPECT_EQ(1u, tabs);
}

#endif  // !defined(OS_CHROMEOS)

}  // namespace
}  // namespace extensions
