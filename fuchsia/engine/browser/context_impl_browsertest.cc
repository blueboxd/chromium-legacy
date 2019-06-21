// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/common.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

using testing::_;
using testing::Field;
using testing::InvokeWithoutArgs;

namespace {

void OnCookiesReceived(net::CookieList* output,
                       base::OnceClosure on_received_cb,
                       const net::CookieList& cookies) {
  *output = cookies;
  std::move(on_received_cb).Run();
}

// Defines a suite of tests that exercise browser-level configuration and
// functionality.
class ContextImplTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  ContextImplTest() = default;
  ~ContextImplTest() override = default;

 protected:
  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  // Synchronously gets a list of cookies for this BrowserContext.
  net::CookieList GetCookies() {
    base::RunLoop run_loop;
    net::CookieList cookies;
    network::mojom::CookieManagerPtr cookie_manager;
    content::BrowserContext::GetDefaultStoragePartition(
        context_impl()->browser_context_for_test())
        ->GetNetworkContext()
        ->GetCookieManager(mojo::MakeRequest(&cookie_manager));
    cookie_manager->GetAllCookies(base::BindOnce(&OnCookiesReceived,
                                                 base::Unretained(&cookies),
                                                 run_loop.QuitClosure()));
    run_loop.Run();
    return cookies;
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextImplTest);
};

}  // namespace

// Verifies that the BrowserContext has a working cookie store by setting
// cookies in the content layer and then querying the CookieStore afterward.
IN_PROC_BROWSER_TEST_F(ContextImplTest, VerifyPersistentCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cookie_url(embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr navigation_controller;
  frame->GetNavigationController(navigation_controller.NewRequest());

  cr_fuchsia::LoadUrlAndExpectResponse(navigation_controller.get(),
                                       fuchsia::web::LoadUrlParams(),
                                       cookie_url.spec());
  navigation_listener_.RunUntilUrlEquals(cookie_url);

  auto cookies = GetCookies();
  bool found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);

  // Check that the cookie persists beyond the lifetime of the Frame by
  // releasing the Frame and re-querying the CookieStore.
  frame.Unbind();
  base::RunLoop().RunUntilIdle();

  found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

// Suite for tests which run the BrowserContext in incognito mode (no data
// directory).
class IncognitoContextImplTest : public ContextImplTest {
 public:
  IncognitoContextImplTest() = default;
  ~IncognitoContextImplTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(kIncognitoSwitch);
    ContextImplTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IncognitoContextImplTest);
};

// Verify that the browser can be initialized without a persistent data
// directory.
IN_PROC_BROWSER_TEST_F(IncognitoContextImplTest, NavigateFrame) {
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url::kAboutBlankURL));
  navigation_listener_.RunUntilUrlEquals(GURL(url::kAboutBlankURL));

  frame.Unbind();
}

IN_PROC_BROWSER_TEST_F(IncognitoContextImplTest, VerifyInMemoryCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cookie_url(embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), cookie_url.spec()));
  navigation_listener_.RunUntilUrlEquals(cookie_url);

  auto cookies = GetCookies();
  bool found = false;
  for (auto c : cookies) {
    if (c.Name() == "foo" && c.Value() == "bar") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}
