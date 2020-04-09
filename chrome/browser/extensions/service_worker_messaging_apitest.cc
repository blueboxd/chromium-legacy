// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class ServiceWorkerMessagingTest : public ExtensionApiTest {
 public:
  ServiceWorkerMessagingTest() = default;
  ~ServiceWorkerMessagingTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  // TODO(lazyboy): Move this to a common place so it can be shared with other
  // tests.
  void StopServiceWorker(const Extension& extension) {
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    content::ServiceWorkerContext* context =
        storage_partition->GetServiceWorkerContext();
    base::RunLoop run_loop;
    // The service worker is registered at the root scope.
    content::StopServiceWorkerForScope(context, extension.url(),
                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  extensions::ScopedTestNativeMessagingHost test_host_;

 private:
  ScopedWorkerBasedExtensionsChannel current_channel_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMessagingTest);
};

// Tests one-way message from content script to SW extension using
// chrome.runtime.sendMessage.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, TabToWorkerOneWay) {
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING", false);
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_tab_to_worker_one_way"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener test_listener("WORKER_RECEIVED_MESSAGE", false);
  test_listener.set_failure_message("FAILURE");

  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents =
        browsertest_util::AddTab(browser(), url);
    EXPECT_TRUE(new_web_contents);
  }

  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
}

// Tests chrome.runtime.sendMessage from content script to SW extension.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, TabToWorker) {
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING", false);
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_tab_to_worker"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener reply_listener("CONTENT_SCRIPT_RECEIVED_REPLY",
                                              false);
  reply_listener.set_failure_message("FAILURE");

  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    const GURL url =
        embedded_test_server()->GetURL("/extensions/test_file.html");
    content::WebContents* new_web_contents =
        browsertest_util::AddTab(browser(), url);
    EXPECT_TRUE(new_web_contents);
  }

  EXPECT_TRUE(reply_listener.WaitUntilSatisfied());
}

// Tests that a message port disconnects if the extension SW is forcefully
// stopped.
// Regression test for https://crbug.com/1033783.
// Disabled due to flake: https://crbug.com/1069550
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       DISABLED_TabToWorker_StopWorkerDisconnects) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Content script disconnect on worker stop test",
           "description": "Tests worker shutdown behavior for messaging",
           "version": "0.1",
           "manifest_version": 2,
           "background": {"service_worker": "service_worker_background.js"},
           "content_scripts": [{
             "matches": ["*://example.com:*/*"],
             "js": ["content_script.js"]
           }]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("service_worker_background.js"),
                     R"(chrome.runtime.onConnect.addListener((port) => {
           console.log('background: runtime.onConnect');
           chrome.test.assertNoLastError();
           chrome.test.notifyPass();
         });
         chrome.test.notifyPass();
      )");
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     R"(var port = chrome.runtime.connect({name:"foo"});
         port.onDisconnect.addListener(() => {
           console.log('content script: port.onDisconnect');
           chrome.test.assertNoLastError();
           chrome.test.notifyPass();
         });
      )");
  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Wait for the extension to register runtime.onConnect listener.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  GURL url =
      embedded_test_server()->GetURL("example.com", "/extensions/body1.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Wait for the content script to connect to the worker's port.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Stop the service worker, this will disconnect the port.
  StopServiceWorker(*extension);

  // Wait for the port to disconnect in the content script.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests chrome.runtime.sendNativeMessage from SW extension to a native
// messaging host.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, NativeMessagingBasic) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));
  ASSERT_TRUE(RunExtensionTest("service_worker/messaging/send_native_message"))
      << message_;
}

// Tests chrome.runtime.connectNative from SW extension to a native messaging
// host.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, ConnectNative) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));
  ASSERT_TRUE(RunExtensionTest("service_worker/messaging/connect_native"))
      << message_;
}

// Tests chrome.tabs.sendMessage from SW extension to content script.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, WorkerToTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("service_worker/messaging/send_message_worker_to_tab"))
      << message_;
}

// Tests port creation (chrome.runtime.connect) from content script to an
// extension SW and disconnecting the port.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       TabToWorker_ConnectAndDisconnect) {
  // Load an extension that will inject content script to |new_web_contents|.
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/connect_and_disconnect"));
  ASSERT_TRUE(extension);

  // Load the tab with content script to open a Port to |extension|.
  // Test concludes when extension gets notified about port being disconnected.
  ResultCatcher catcher;
  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    content::WebContents* new_web_contents = browsertest_util::AddTab(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests port creation (chrome.runtime.connect) from content script to an
// extension and sending message through the port.
// TODO(lazyboy): Refactor common parts with TabToWorker_ConnectAndDisconnect.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest,
                       TabToWorker_ConnectAndPostMessage) {
  // Load an extension that will inject content script to |new_web_contents|.
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_to_worker/post_message"));
  ASSERT_TRUE(extension);

  // Load the tab with content script to send message to |extension| via port.
  // Test concludes when the content script receives a reply.
  ResultCatcher catcher;
  {
    ASSERT_TRUE(StartEmbeddedTestServer());
    content::WebContents* new_web_contents = browsertest_util::AddTab(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.html"));
    EXPECT_TRUE(new_web_contents);
  }
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests chrome.runtime.onMessageExternal between two Service Worker based
// extensions.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, ExternalMessageToWorker) {
  const std::string kTargetExtensionId = "pkplfbidichfdicaijlchgnapepdginl";

  // Load the receiver extension first.
  const Extension* target_extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/send_message_external/target"));
  ASSERT_TRUE(target_extension);
  EXPECT_EQ(kTargetExtensionId, target_extension->id());

  // Then run the test from initiator extension.
  ASSERT_TRUE(RunExtensionTest(
      "service_worker/messaging/send_message_external/initiator"))
      << message_;
}

// Tests chrome.runtime.onConnectExternal between two Service Worker based
// extensions.
IN_PROC_BROWSER_TEST_F(ServiceWorkerMessagingTest, ConnectExternalToWorker) {
  const std::string kTargetExtensionId = "pkplfbidichfdicaijlchgnapepdginl";

  // Load the receiver extension first.
  const Extension* target_extension = LoadExtension(test_data_dir_.AppendASCII(
      "service_worker/messaging/connect_external/target"));
  ASSERT_TRUE(target_extension);
  EXPECT_EQ(kTargetExtensionId, target_extension->id());

  // Then run the test from initiator extension.
  ASSERT_TRUE(
      RunExtensionTest("service_worker/messaging/connect_external/initiator"))
      << message_;
}

}  // namespace extensions
