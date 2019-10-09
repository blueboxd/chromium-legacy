// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

namespace {

void GetResource(const std::string& id,
                 const WebUIDataSource::GotDataCallback& callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string contents;
  base::FilePath path;
  CHECK(base::PathService::Get(content::DIR_TEST_DATA, &path));
  path = path.AppendASCII(id.substr(0, id.find("?")));
  CHECK(base::ReadFileToString(path, &contents)) << path.value();

  base::RefCountedString* ref_contents = new base::RefCountedString;
  ref_contents->data() = contents;
  callback.Run(ref_contents);
}

struct WebUIControllerConfig {
  int bindings = BINDINGS_POLICY_MOJO_WEB_UI;
  std::string child_src = "child-src 'self' chrome://web-ui-subframe/;";
  bool disable_xfo = false;
};

class TestWebUIController : public WebUIController {
 public:
  TestWebUIController(WebUI* web_ui,
                      const GURL& base_url,
                      const WebUIControllerConfig& config)
      : WebUIController(web_ui) {
    web_ui->SetBindings(config.bindings);

    WebUIDataSource* data_source = WebUIDataSource::Create(base_url.host());
    data_source->SetRequestFilter(
        base::BindRepeating([](const std::string& path) { return true; }),
        base::BindRepeating(&GetResource));

    if (!config.child_src.empty())
      data_source->OverrideContentSecurityPolicyChildSrc(config.child_src);

    if (config.disable_xfo)
      data_source->DisableDenyXFrameOptions();

    WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                         data_source);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIController);
};

class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() {}

  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    if (!url.SchemeIs(kChromeUIScheme))
      return nullptr;

    WebUIControllerConfig config;
    if (url.has_query()) {
      std::string value;
      bool has_value = net::GetValueForKeyInQuery(url, "bindings", &value);
      if (has_value)
        EXPECT_TRUE(base::StringToInt(value, &(config.bindings)));

      has_value = net::GetValueForKeyInQuery(url, "noxfo", &value);
      if (has_value && value == "true")
        config.disable_xfo = true;
    }

    return std::make_unique<TestWebUIController>(web_ui, url, config);
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    if (!url.SchemeIs(kChromeUIScheme))
      return WebUI::kNoWebUI;

    return reinterpret_cast<WebUI::TypeID>(base::Hash(url.host()));
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
  }
  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) override {
    return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIControllerFactory);
};

}  // namespace

class WebUISecurityTest : public ContentBrowserTest {
 public:
  WebUISecurityTest() { WebUIControllerFactory::RegisterFactory(&factory_); }

  ~WebUISecurityTest() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  TestWebUIControllerFactory* factory() { return &factory_; }

 private:
  TestWebUIControllerFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUISecurityTest);
};

// Loads a WebUI which does not have any bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, NoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=0"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
}

// Loads a WebUI which has WebUI bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
}

// Loads a WebUI which has Mojo bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, MojoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_MOJO_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
}

// Loads a WebUI which has both WebUI and Mojo bindings.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUIAndMojoBindings) {
  GURL test_url(GetWebUIURL("web-ui/title1.html?bindings=" +
                            base::NumberToString(BINDINGS_POLICY_WEB_UI |
                                                 BINDINGS_POLICY_MOJO_WEB_UI)));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID()));
}

// Verify that a WebUI can add a subframe for its own WebUI.
// Note: This works by accident, since the main frame WebUI will respond to
// the navigation request for the subframe. See https://crbug.com/713313.
IN_PROC_BROWSER_TEST_F(WebUISecurityTest, WebUISameSiteSubframe) {
  // TODO(nasko): Remove the noxfo=true parameter when https://crbug.com/713313
  // is fixed. It is required for now, since currently WebUIs are not created
  // for subframes, so the main frame WebUI object will respond to the
  // navigation request for the subframe, which means we need to disable
  // X-Frame-Options on it.
  GURL test_url(GetWebUIURL("web-ui/page_with_blank_iframe.html?noxfo=true"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(1U, root->child_count());

  TestFrameNavigationObserver observer(root->child_at(0));
  GURL subframe_url(GetWebUIURL("web-ui/title1.html?noxfo=true"));
  NavigateFrameToURL(root->child_at(0), subframe_url);

  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(subframe_url, observer.last_committed_url());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(
      GetWebUIURL("web-ui"),
      root->child_at(0)->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // TODO(nasko): The subframe should have its own WebUI object, so the
  // following expectation should be inverted once https://crbug.com/713313 is
  // fixed.
  EXPECT_EQ(nullptr, root->child_at(0)->current_frame_host()->web_ui());
  EXPECT_NE(root->current_frame_host()->web_ui(),
            root->child_at(0)->current_frame_host()->web_ui());
}

}  // namespace content
