// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// PermissionManager subclass that enables the test below to deterministically
// wait until there is a permission status subscription from a service worker.
// Deleting the off-the-record profile under these circumstances would
// previously have resulted in a crash.
class SubscriptionInterceptingPermissionManager : public PermissionManager {
 public:
  explicit SubscriptionInterceptingPermissionManager(Profile* profile)
      : PermissionManager(profile) {}

  ~SubscriptionInterceptingPermissionManager() override = default;

  void SetSubscribeCallback(base::RepeatingClosure callback) {
    callback_ = std::move(callback);
  }

  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override {
    int result = PermissionManager::SubscribePermissionStatusChange(
        permission, render_frame_host, requesting_origin, callback);
    std::move(callback_).Run();

    return result;
  }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

class PermissionManagerBrowserTest : public InProcessBrowserTest {
 public:
  PermissionManagerBrowserTest() = default;

  ~PermissionManagerBrowserTest() override = default;

  static std::unique_ptr<KeyedService> CreateTestingPermissionManager(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<SubscriptionInterceptingPermissionManager>(profile);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    incognito_browser_ = CreateIncognitoBrowser();

    PermissionManagerFactory::GetInstance()->SetTestingFactory(
        incognito_browser_->profile(),
        base::BindRepeating(
            &PermissionManagerBrowserTest::CreateTestingPermissionManager));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Browser* incognito_browser() { return incognito_browser_; }

 private:
  Browser* incognito_browser_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(PermissionManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PermissionManagerBrowserTest,
                       ServiceWorkerPermissionQueryIncognitoClose) {
  base::RunLoop run_loop;
  PermissionManager* pm =
      PermissionManagerFactory::GetForProfile(incognito_browser()->profile());
  static_cast<SubscriptionInterceptingPermissionManager*>(pm)
      ->SetSubscribeCallback(run_loop.QuitClosure());

  ui_test_utils::NavigateToURL(
      incognito_browser(), embedded_test_server()->GetURL(
                               "/permissions/permissions_service_worker.html"));
  run_loop.Run();

  // TODO(crbug.com/889276) : We are relying here on the test shuts down to
  // close the browser. We need to make the test more robust by closing the
  // browser explicitly.
}
