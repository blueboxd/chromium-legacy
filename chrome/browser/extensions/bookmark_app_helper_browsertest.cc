// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace extensions {
namespace {

// Extends BookmarkAppHelper to see the call to OnIconsDownloaded.
class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(Profile* profile,
                        const WebApplicationInfo& web_app_info,
                        content::WebContents* contents,
                        base::Closure on_icons_downloaded_closure,
                        WebappInstallSource install_source)
      : BookmarkAppHelper(profile,
                          std::make_unique<WebApplicationInfo>(web_app_info),
                          contents,
                          install_source),
        on_icons_downloaded_closure_(on_icons_downloaded_closure) {}

  // TestBookmarkAppHelper:
  void OnIconsDownloaded(
      bool success,
      const std::map<GURL, std::vector<SkBitmap>>& bitmaps) override {
    on_icons_downloaded_closure_.Run();
    BookmarkAppHelper::OnIconsDownloaded(success, bitmaps);
  }

 private:
  base::Closure on_icons_downloaded_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

}  // namespace

class BookmarkAppHelperTest : public DialogBrowserTest,
                              public extensions::ExtensionRegistryObserver {
 public:
  BookmarkAppHelperTest() {}

  content::WebContents* web_contents() { return web_contents_; }

  void OnDidGetWebApplicationInfo(
      chrome::mojom::ChromeRenderFrameAssociatedPtr chrome_render_frame,
      const WebApplicationInfo& const_info) {
    WebApplicationInfo info = const_info;

    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();

    // Mimic extensions::TabHelper for fields missing from the manifest.
    if (info.app_url.is_empty())
      info.app_url = web_contents_->GetURL();
    if (info.title.empty())
      info.title = web_contents_->GetTitle();
    if (info.title.empty())
      info.title = base::UTF8ToUTF16(info.app_url.spec());

    bookmark_app_helper_ = std::make_unique<TestBookmarkAppHelper>(
        browser()->profile(), info, web_contents_, quit_closure_,
        WebappInstallSource::MENU_BROWSER_TAB);
    bookmark_app_helper_->Create(
        base::Bind(&BookmarkAppHelperTest::FinishCreateBookmarkApp,
                   base::Unretained(this)));
  }

  void FinishCreateBookmarkApp(const Extension* extension,
                               const WebApplicationInfo& web_app_info) {
    // ~WebAppReadyMsgWatcher() is called on the IO thread, but
    // |bookmark_app_helper_| must be destroyed on the UI thread.
    bookmark_app_helper_.reset();
    // Web contents reparenting happens only after shortcut creation has
    // completed.
    quit_closure_.Run();
  }

  void Wait() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // ExtensionRegistryObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override {
    if (!expected_app_title_.empty())
      EXPECT_EQ(expected_app_title_, extension->name());
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(embedded_test_server()->Start());

    const std::string path = (name == "CreateWindowedPWA")
                                 ? "/banners/manifest_test_page.html"
                                 : "/favicon/page_with_favicon.html";
    AddTabAtIndex(1, GURL(embedded_test_server()->GetURL(path)),
                  ui::PAGE_TRANSITION_LINK);

    chrome::mojom::ChromeRenderFrameAssociatedPtr chrome_render_frame;
    browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&chrome_render_frame);
    // Bind the InterfacePtr into the callback so that it's kept alive
    // until there's either a connection error or a response.
    auto* web_app_info_proxy = chrome_render_frame.get();
    web_app_info_proxy->GetWebApplicationInfo(
        base::Bind(&BookmarkAppHelperTest::OnDidGetWebApplicationInfo,
                   base::Unretained(this), base::Passed(&chrome_render_frame)));
    Wait();
  }

  void SetExpectedAppTitle(const std::string& expected_title) {
    expected_app_title_ = expected_title;
  }

 protected:
  std::unique_ptr<TestBookmarkAppHelper> bookmark_app_helper_;

 private:
  base::Closure quit_closure_;
  content::WebContents* web_contents_ = nullptr;
  std::string expected_app_title_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperTest);
};

// Launches an installation confirmation dialog for a PWA.
// Deprecated in favour of
// InstallManagerBookmarkAppDialogTest.CreateWindowedPWA_FromNonInstallableSite.
// TODO(crbug.com/915043): Erase it.
IN_PROC_BROWSER_TEST_F(BookmarkAppHelperTest, CreateWindowedPWA) {
  ShowAndVerifyUi();
}

// Runs through a complete installation of a PWA and ensures the tab is
// reparented into an app window.
// Deprecated in favour of
// InstallManagerBookmarkAppDialogTest.CreateWindowedPWA_FromInstallableSite.
// TODO(crbug.com/915043): Erase it.
IN_PROC_BROWSER_TEST_F(BookmarkAppHelperTest, CreateWindowedPWAIntoAppWindow) {
  // The PWA dialog will be launched because manifest_test_page.html passes
  // the PWA check.
  SetExpectedAppTitle("Manifest test app");

  ShowUi("CreateWindowedPWA");

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer(this);
  observer.Add(ExtensionRegistry::Get(browser()->profile()));
  bookmark_app_helper_->OnBubbleCompleted(
      true, std::make_unique<WebApplicationInfo>(
                bookmark_app_helper_->web_app_info_));
  Wait();  // Quits when the extension install completes.

  Browser* app_browser = chrome::FindBrowserWithWebContents(web_contents());
  EXPECT_TRUE(app_browser->is_app());
  EXPECT_NE(app_browser, browser());
}

}  // namespace extensions
