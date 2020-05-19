// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_web_view_impl.h"

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_web_view.h"
#include "ash/public/cpp/assistant/assistant_web_view_factory.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/ash/assistant/assistant_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace assistant {

namespace {

using ash::AssistantWebView;
using ash::AssistantWebViewFactory;

// Please remember to set auth token when *not* running in |kReplay| mode.
constexpr auto kMode = FakeS3Mode::kReplay;

// Update this when you introduce breaking changes to existing tests.
constexpr int kVersion = 1;

// Helpers ---------------------------------------------------------------------

std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;

  widget->Init(std::move(params));
  return widget;
}

GURL CreateDataUrlWithBody(const std::string& body) {
  return GURL(base::StringPrintf(R"(data:text/html,
      <html>
        <body>
          <style>
            * {
              margin: 0;
              padding: 0;
            }
          </style>
          %s
        </body>
      </html>
    )",
                                 body.c_str()));
}

GURL CreateDataUrl() {
  return CreateDataUrlWithBody(std::string());
}

// Mocks -----------------------------------------------------------------------

class MockViewObserver : public testing::NiceMock<views::ViewObserver> {
 public:
  // views::ViewObserver:
  MOCK_METHOD(void,
              OnViewPreferredSizeChanged,
              (views::View * view),
              (override));
};

class MockAssistantWebViewObserver
    : public testing::NiceMock<AssistantWebView::Observer> {
 public:
  // AssistantWebView::Observer:
  MOCK_METHOD(void, DidStopLoading, (), (override));

  MOCK_METHOD(void,
              DidSuppressNavigation,
              (const GURL& url,
               WindowOpenDisposition disposition,
               bool from_user_gesture),
              (override));

  MOCK_METHOD(void, DidChangeCanGoBack, (bool can_go_back), (override));
};

// Expectations ----------------------------------------------------------------

void ExpectPreferredSize(AssistantWebView* web_view,
                         const gfx::Size& expected_preferred_size) {
  MockViewObserver mock;
  ScopedObserver<views::View, views::ViewObserver> observer{&mock};
  observer.Add(static_cast<views::View*>(web_view));

  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnViewPreferredSizeChanged)
      .WillOnce(testing::Invoke([&](views::View* view) {
        EXPECT_EQ(expected_preferred_size, view->GetPreferredSize());
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
}

void ExpectDidStopLoading(AssistantWebView* web_view) {
  MockAssistantWebViewObserver mock;
  ScopedObserver<AssistantWebView, AssistantWebView::Observer> obs{&mock};
  obs.Add(web_view);

  base::RunLoop run_loop;
  EXPECT_CALL(mock, DidStopLoading).WillOnce(testing::Invoke([&run_loop]() {
    run_loop.QuitClosure().Run();
  }));
  run_loop.Run();
}

void ExpectDidSuppressNavigation(AssistantWebView* web_view,
                                 const GURL& expected_url,
                                 WindowOpenDisposition expected_disposition,
                                 bool expected_from_user_gesture) {
  MockAssistantWebViewObserver mock;
  ScopedObserver<AssistantWebView, AssistantWebView::Observer> obs{&mock};
  obs.Add(web_view);

  base::RunLoop run_loop;
  EXPECT_CALL(mock, DidSuppressNavigation)
      .WillOnce(testing::Invoke([&](const GURL& url,
                                    WindowOpenDisposition disposition,
                                    bool from_user_gesture) {
        EXPECT_EQ(expected_url, url);
        EXPECT_EQ(expected_disposition, disposition);
        EXPECT_EQ(expected_from_user_gesture, from_user_gesture);
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
}

void ExpectDidChangeCanGoBack(AssistantWebView* web_view,
                              bool expected_can_go_back) {
  MockAssistantWebViewObserver mock;
  ScopedObserver<AssistantWebView, AssistantWebView::Observer> obs{&mock};
  obs.Add(web_view);

  base::RunLoop run_loop;
  EXPECT_CALL(mock, DidChangeCanGoBack)
      .WillOnce(testing::Invoke([&](bool can_go_back) {
        EXPECT_EQ(expected_can_go_back, can_go_back);
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
}

}  // namespace

// AssistantWebViewImplBrowserTest ---------------------------------------------

class AssistantWebViewImplBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  AssistantWebViewImplBrowserTest() = default;
  ~AssistantWebViewImplBrowserTest() override = default;

 private:
  AssistantTestMixin tester_{&mixin_host_, this, embedded_test_server(), kMode,
                             kVersion};
};

// Tests -----------------------------------------------------------------------

// Tests that AssistantWebViewImpl will automatically update its preferred size
// to match the desired size of its hosted contents.
IN_PROC_BROWSER_TEST_F(AssistantWebViewImplBrowserTest, ShouldAutoResize) {
  AssistantWebView::InitParams params;
  params.enable_auto_resize = true;
  params.min_size = gfx::Size(600, 400);
  params.max_size = gfx::Size(800, 600);

  auto widget = CreateWidget();
  AssistantWebView* web_view =
      widget->SetContentsView(AssistantWebViewFactory::Get()->Create(params));

  // Verify auto-resizing within min/max bounds.
  web_view->Navigate(
      CreateDataUrlWithBody("<div style='width:700px; height:500px'></div>"));
  ExpectPreferredSize(web_view, gfx::Size(700, 500));

  // Verify auto-resizing clamps to min bounds.
  web_view->Navigate(
      CreateDataUrlWithBody("<div style='width:0; height:0'></div>"));
  ExpectPreferredSize(web_view, gfx::Size(600, 400));

  // Verify auto-resizing clamps to max bounds.
  web_view->Navigate(
      CreateDataUrlWithBody("<div style='width:1000px; height:1000px'></div>"));
  ExpectPreferredSize(web_view, gfx::Size(800, 600));
}

// Tests that AssistantWebViewImpl will notify DidStopLoading() events.
IN_PROC_BROWSER_TEST_F(AssistantWebViewImplBrowserTest,
                       ShouldNotifyDidStopLoading) {
  auto widget = CreateWidget();
  AssistantWebView* web_view = widget->SetContentsView(
      AssistantWebViewFactory::Get()->Create(AssistantWebView::InitParams()));

  web_view->Navigate(CreateDataUrl());
  ExpectDidStopLoading(web_view);
}

// Tests that AssistantWebViewImpl will notify DidSuppressNavigation() events.
IN_PROC_BROWSER_TEST_F(AssistantWebViewImplBrowserTest,
                       ShouldNotifyDidSuppressNavigation) {
  AssistantWebView::InitParams params;
  params.suppress_navigation = true;

  auto widget = CreateWidget();
  AssistantWebView* web_view = widget->SetContentsView(
      AssistantWebViewFactory::Get()->Create(std::move(params)));

  web_view->Navigate(CreateDataUrlWithBody(R"(
      <script>
        // Wait until window has finished loading.
        window.addEventListener("load", () => {

          // Perform simple click on an anchor within the same target.
          const anchor = document.createElement("a");
          anchor.href = "https://google.com/";
          anchor.click();

          // Wait for first click event to be flushed.
          setTimeout(() => {

            // Perform simple click on an anchor with "_blank" target.
            const anchor = document.createElement("a");
            anchor.href = "https://assistant.google.com/";
            anchor.target = "_blank";
            anchor.click();
          }, 0);
        });
      </script>
    )"));

  // Expect suppression of the first click event.
  ExpectDidSuppressNavigation(
      web_view, /*url=*/GURL("https://google.com/"),
      /*disposition=*/WindowOpenDisposition::CURRENT_TAB,
      /*from_user_gesture=*/false);

  // Expect suppression of the second click event.
  ExpectDidSuppressNavigation(
      web_view, /*url=*/GURL("https://assistant.google.com/"),
      /*disposition=*/WindowOpenDisposition::NEW_FOREGROUND_TAB,
      /*from_user_gesture=*/true);
}

// Tests that AssistantWebViewImpl will notify DidChangeCanGoBack() events.
IN_PROC_BROWSER_TEST_F(AssistantWebViewImplBrowserTest,
                       ShouldNotifyDidChangeCanGoBack) {
  auto widget = CreateWidget();
  AssistantWebView* web_view = widget->SetContentsView(
      AssistantWebViewFactory::Get()->Create(AssistantWebView::InitParams()));

  web_view->Navigate(CreateDataUrlWithBody("<div>First Page</div>"));
  ExpectDidStopLoading(web_view);

  web_view->Navigate(CreateDataUrlWithBody("<div>Second Page</div>"));
  ExpectDidChangeCanGoBack(web_view, /*can_go_back=*/true);

  web_view->GoBack();
  ExpectDidChangeCanGoBack(web_view, /*can_go_back=*/false);
}

}  // namespace assistant
}  // namespace chromeos
