// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_utils.h"

namespace {

constexpr char kTabUrl1[] = "http://foo/1";
constexpr char kTabUrl2[] = "http://foo/2";
constexpr char kTabUrl3[] = "http://foo/3";
constexpr char kTabUrl4[] = "http://foo/4";

constexpr char kTabUrlName1[] = "foo/1";
constexpr char kTabUrlName2[] = "foo/2";
constexpr char kTabUrlName3[] = "foo/3";

constexpr char kTabName1[] = "Tab 1";
constexpr char kTabName2[] = "Tab 2";
constexpr char kTabName3[] = "Tab 3";
constexpr char kTabName4[] = "Tab 4";

class MockPage : public tab_search::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<tab_search::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<tab_search::mojom::Page> receiver_{this};
};

void ExpectNewTab(const tab_search::mojom::Tab* tab,
                  const std::string url,
                  const std::string title,
                  int index) {
  EXPECT_EQ(index, tab->index);
  EXPECT_LT(0, tab->tab_id);
  EXPECT_FALSE(tab->group_id.has_value());
  EXPECT_FALSE(tab->pinned);
  EXPECT_EQ(title, tab->title);
  EXPECT_EQ(url, tab->url);
  EXPECT_TRUE(tab->fav_icon_url.has_value());
  EXPECT_TRUE(tab->is_default_favicon);
  EXPECT_TRUE(tab->show_icon);
}

void ExpectProfileTabs(tab_search::mojom::ProfileTabs* profile_tabs) {
  ASSERT_EQ(2u, profile_tabs->windows.size());
  auto* window1 = profile_tabs->windows[0].get();
  ASSERT_EQ(2u, window1->tabs.size());
  ASSERT_FALSE(window1->tabs[0]->active);
  ASSERT_TRUE(window1->tabs[1]->active);
  auto* window2 = profile_tabs->windows[1].get();
  ASSERT_EQ(1u, window2->tabs.size());
  ASSERT_TRUE(window2->tabs[0]->active);
}

class TestTabSearchPageHandler : public TabSearchPageHandler {
 public:
  TestTabSearchPageHandler(mojo::PendingRemote<tab_search::mojom::Page> page,
                           content::WebUI* web_ui)
      : TabSearchPageHandler(
            mojo::PendingReceiver<tab_search::mojom::PageHandler>(),
            std::move(page),
            web_ui) {}
};

class TabSearchPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    browser2_ = CreateTestBrowser(false, false);
    browser3_ = CreateTestBrowser(true, false);
    BrowserList::SetLastActive(browser1());
    handler_ = std::make_unique<TestTabSearchPageHandler>(
        page_.BindAndGetRemote(), web_ui());
  }

  void TearDown() override {
    browser1()->tab_strip_model()->CloseAllTabs();
    browser2()->tab_strip_model()->CloseAllTabs();
    browser3()->tab_strip_model()->CloseAllTabs();
    browser2_.reset();
    browser3_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Browser* browser1() { return browser(); }
  Browser* browser2() { return browser2_.get(); }
  Browser* browser3() { return browser3_.get(); }
  TestTabSearchPageHandler* handler() { return handler_.get(); }

 protected:
  void AddTabWithTitle(Browser* browser,
                       const GURL url,
                       const std::string title) {
    AddTab(browser, url);
    NavigateAndCommitActiveTabWithTitle(browser, url,
                                        base::ASCIIToUTF16(title));
  }

 private:
  std::unique_ptr<Browser> CreateTestBrowser(bool incognito, bool popup) {
    auto window = std::make_unique<TestBrowserWindow>();
    Profile* profile = incognito ? browser()->profile()->GetPrimaryOTRProfile()
                                 : browser()->profile();
    Browser::Type type = popup ? Browser::TYPE_POPUP : Browser::TYPE_NORMAL;

    std::unique_ptr<Browser> browser =
        CreateBrowser(profile, type, false, window.get());
    BrowserList::SetLastActive(browser.get());
    new TestBrowserWindowOwner(window.release());
    return browser;
  }

  testing::StrictMock<MockPage> page_;
  content::TestWebUI web_ui_;
  std::unique_ptr<Browser> browser2_;
  std::unique_ptr<Browser> browser3_;
  std::unique_ptr<TestTabSearchPageHandler> handler_;
};

TEST_F(TabSearchPageHandlerTest, GetTabs) {
  // Browser3 is in incognito mode, thus its tab should not be accessible.
  AddTabWithTitle(browser3(), GURL(kTabUrl4), kTabName4);
  AddTabWithTitle(browser2(), GURL(kTabUrl3), kTabName3);
  AddTabWithTitle(browser1(), GURL(kTabUrl2), kTabName2);
  AddTabWithTitle(browser1(), GURL(kTabUrl1), kTabName1);

  int32_t tab_id2 = 0;
  int32_t tab_id3 = 0;

  // Get Tabs.
  tab_search::mojom::PageHandler::GetProfileTabsCallback callback1 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileTabsPtr profile_tabs) {
            ASSERT_EQ(2u, profile_tabs->windows.size());
            auto* window1 = profile_tabs->windows[0].get();
            ASSERT_TRUE(window1->active);
            ASSERT_EQ(2u, window1->tabs.size());

            auto* tab1 = window1->tabs[0].get();
            ExpectNewTab(tab1, kTabUrlName1, kTabName1, 0);
            ASSERT_TRUE(tab1->active);

            auto* tab2 = window1->tabs[1].get();
            ExpectNewTab(tab2, kTabUrlName2, kTabName2, 1);
            ASSERT_FALSE(tab2->active);

            auto* window2 = profile_tabs->windows[1].get();
            ASSERT_FALSE(window2->active);
            ASSERT_EQ(1u, window2->tabs.size());

            auto* tab3 = window2->tabs[0].get();
            ExpectNewTab(tab3, kTabUrlName3, kTabName3, 0);
            ASSERT_TRUE(tab3->active);

            tab_id2 = tab2->tab_id;
            tab_id3 = tab3->tab_id;
          });
  handler()->GetProfileTabs(std::move(callback1));

  // Switch to 2nd tab.
  auto switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id2;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  // Get Tabs again to verify tab switch.
  tab_search::mojom::PageHandler::GetProfileTabsCallback callback2 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileTabsPtr profile_tabs) {
            ExpectProfileTabs(profile_tabs.get());
          });
  handler()->GetProfileTabs(std::move(callback2));

  // Switch to 3rd tab.
  switch_to_tab_info = tab_search::mojom::SwitchToTabInfo::New();
  switch_to_tab_info->tab_id = tab_id3;
  handler()->SwitchToTab(std::move(switch_to_tab_info));

  // Get Tabs again to verify tab switch.
  tab_search::mojom::PageHandler::GetProfileTabsCallback callback3 =
      base::BindLambdaForTesting(
          [&](tab_search::mojom::ProfileTabsPtr profile_tabs) {
            ExpectProfileTabs(profile_tabs.get());
          });
  handler()->GetProfileTabs(std::move(callback3));
}

}  // namespace
