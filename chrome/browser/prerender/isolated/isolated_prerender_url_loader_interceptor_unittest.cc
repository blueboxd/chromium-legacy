// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader_interceptor.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const gfx::Size kSize(640, 480);

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL TestURL() {
  return GURL("https://test.com/path");
}

}  // namespace

// These tests leak mojo objects (like the IsolatedPrerenderURLLoader) because
// they do not have valid mojo channels, which would normally delete the bound
// objects on destruction. This is expected and cannot be easily fixed without
// rewriting these as browsertests. The trade off for the speed and flexibility
// of unittests is an intentional decision.
#if defined(LEAK_SANITIZER)
#define DISABLE_ASAN(x) DISABLED_##x
#else
#define DISABLE_ASAN(x) x
#endif

class IsolatedPrerenderURLLoaderInterceptorTest
    : public ChromeRenderViewHostTestHarness {
 public:
  IsolatedPrerenderURLLoaderInterceptorTest() = default;
  ~IsolatedPrerenderURLLoaderInterceptorTest() override = default;

  void TearDown() override {
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile());
    prerender_manager->CancelAllPrerenders();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<prerender::PrerenderHandle> StartPrerender(const GURL& url) {
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile());

    return prerender_manager->AddPrerenderFromNavigationPredictor(
        url,
        web_contents()->GetController().GetDefaultSessionStorageNamespace(),
        kSize);
  }

  void WaitForCallback() {
    if (was_intercepted_.has_value())
      return;

    base::RunLoop run_loop;
    waiting_for_callback_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void HandlerCallback(
      content::URLLoaderRequestInterceptor::RequestHandler callback) {
    was_intercepted_ = !callback.is_null();
    if (waiting_for_callback_closure_) {
      std::move(waiting_for_callback_closure_).Run();
    }
  }

  base::Optional<bool> was_intercepted() { return was_intercepted_; }

 private:
  base::Optional<bool> was_intercepted_;
  base::OnceClosure waiting_for_callback_closure_;
};

TEST_F(IsolatedPrerenderURLLoaderInterceptorTest, DISABLE_ASAN(WantIntercept)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  std::unique_ptr<prerender::PrerenderHandle> handle =
      StartPrerender(TestURL());

  std::unique_ptr<IsolatedPrerenderURLLoaderInterceptor> interceptor =
      std::make_unique<IsolatedPrerenderURLLoaderInterceptor>(
          handle->contents()
              ->prerender_contents()
              ->GetMainFrame()
              ->GetFrameTreeNodeId());

  network::ResourceRequest request;
  request.url = TestURL();
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor->MaybeCreateLoader(
      request, profile(),
      base::BindOnce(
          &IsolatedPrerenderURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_TRUE(was_intercepted().value());
}

TEST_F(IsolatedPrerenderURLLoaderInterceptorTest, DISABLE_ASAN(FeatureOff)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatePrerenders);

  std::unique_ptr<prerender::PrerenderHandle> handle =
      StartPrerender(TestURL());

  std::unique_ptr<IsolatedPrerenderURLLoaderInterceptor> interceptor =
      std::make_unique<IsolatedPrerenderURLLoaderInterceptor>(
          handle->contents()
              ->prerender_contents()
              ->GetMainFrame()
              ->GetFrameTreeNodeId());

  network::ResourceRequest request;
  request.url = TestURL();
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor->MaybeCreateLoader(
      request, profile(),
      base::BindOnce(
          &IsolatedPrerenderURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_FALSE(was_intercepted().value());
}

TEST_F(IsolatedPrerenderURLLoaderInterceptorTest, DISABLE_ASAN(NotAPrerender)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  std::unique_ptr<IsolatedPrerenderURLLoaderInterceptor> interceptor =
      std::make_unique<IsolatedPrerenderURLLoaderInterceptor>(
          web_contents()->GetMainFrame()->GetFrameTreeNodeId());

  network::ResourceRequest request;
  request.url = TestURL();
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor->MaybeCreateLoader(
      request, profile(),
      base::BindOnce(
          &IsolatedPrerenderURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_FALSE(was_intercepted().value());
}

TEST_F(IsolatedPrerenderURLLoaderInterceptorTest, DISABLE_ASAN(NotAFrame)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kIsolatePrerenders);

  std::unique_ptr<IsolatedPrerenderURLLoaderInterceptor> interceptor =
      std::make_unique<IsolatedPrerenderURLLoaderInterceptor>(1337);

  network::ResourceRequest request;
  request.url = TestURL();
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor->MaybeCreateLoader(
      request, profile(),
      base::BindOnce(
          &IsolatedPrerenderURLLoaderInterceptorTest::HandlerCallback,
          base::Unretained(this)));
  WaitForCallback();

  EXPECT_TRUE(was_intercepted().has_value());
  EXPECT_FALSE(was_intercepted().value());
}
