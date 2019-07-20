// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_content_browsertest.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

using Microsoft::WRL::ComPtr;

namespace content {

#define EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(safearray, expected_property_values) \
  {                                                                         \
    EXPECT_EQ(sizeof(V_R8(LPVARIANT(NULL))),                                \
              ::SafeArrayGetElemsize(safearray));                           \
    ASSERT_EQ(1u, SafeArrayGetDim(safearray));                              \
    LONG array_lower_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));              \
    LONG array_upper_bound;                                                 \
    ASSERT_HRESULT_SUCCEEDED(                                               \
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));              \
    double* array_data;                                                     \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(                         \
        safearray, reinterpret_cast<void**>(&array_data)));                 \
    size_t count = array_upper_bound - array_lower_bound + 1;               \
    ASSERT_EQ(expected_property_values.size(), count);                      \
    for (size_t i = 0; i < count; ++i) {                                    \
      EXPECT_EQ(array_data[i], expected_property_values[i]);                \
    }                                                                       \
    ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(safearray));           \
  }

#define EXPECT_UIA_TEXTRANGE_EQ(provider, expected_content) \
  {                                                         \
    base::win::ScopedBstr provider_content;                 \
    ASSERT_HRESULT_SUCCEEDED(                               \
        provider->GetText(-1, provider_content.Receive())); \
    EXPECT_STREQ(expected_content, provider_content);       \
  }

#define EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider, endpoint, unit,  \
                                         count, expected_text, expected_count) \
  {                                                                            \
    int result_count;                                                          \
    EXPECT_HRESULT_SUCCEEDED(text_range_provider->MoveEndpointByUnit(          \
        endpoint, unit, count, &result_count));                                \
    EXPECT_EQ(expected_count, result_count);                                   \
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, expected_text);               \
  }

#define EXPECT_UIA_MOVE(text_range_provider, unit, count, expected_text, \
                        expected_count)                                  \
  {                                                                      \
    int result_count;                                                    \
    EXPECT_HRESULT_SUCCEEDED(                                            \
        text_range_provider->Move(unit, count, &result_count));          \
    EXPECT_EQ(expected_count, result_count);                             \
    EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, expected_text);         \
  }

class AXPlatformNodeTextRangeProviderWinBrowserTest
    : public AccessibilityContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void SynchronizeThreads() {
    MainThreadFrameObserver observer(GetWidgetHost());
    observer.Wait();
  }

  void GetTextRangeProviderFromTextNode(
      ComPtr<ITextRangeProvider>& text_range_provider,
      BrowserAccessibility* target_browser_accessibility) {
    auto* provider_simple =
        ToBrowserAccessibilityWin(target_browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, provider_simple);

    ComPtr<ITextProvider> text_provider;
    EXPECT_HRESULT_SUCCEEDED(
        provider_simple->GetPatternProvider(UIA_TextPatternId, &text_provider));
    ASSERT_NE(nullptr, text_provider.Get());

    EXPECT_HRESULT_SUCCEEDED(
        text_provider->get_DocumentRange(&text_range_provider));
    ASSERT_NE(nullptr, text_range_provider.Get());
  }

  // Run through ITextRangeProvider::ScrollIntoView top tests. It's assumed that
  // the browser has already loaded an HTML document's accessibility tree.
  // Assert the text range generated for an accessibility node is scrolled to be
  // flush with the top of the viewport.
  //   expected_start_role: the expected accessibility role of the text range
  //                        start node under test
  //   fstart:              the function to retrieve the accessibility text
  //                        range start node under test from the root
  //                        accessibility node
  //   expected_end_role:   the expected accessibility role of the text range
  //                        end node under test
  //   fend:                the function to retrieve the accessibility text
  //                        range end node under test from the root
  //                        accessibility node
  //   align_to_top:        true to test top viewport alignment, otherwise test
  //                        bottom viewport alignment
  void ScrollIntoViewBrowserTestTemplate(
      const ax::mojom::Role expected_start_role,
      BrowserAccessibility* (BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_end_role,
      BrowserAccessibility* (BrowserAccessibility::*fend)() const,
      const bool align_to_top) {
    BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();

    BrowserAccessibility* browser_accessibility_start =
        (root_browser_accessibility->*fstart)();
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    BrowserAccessibility* browser_accessibility_end =
        (root_browser_accessibility->*fend)();
    ASSERT_NE(nullptr, browser_accessibility_end);
    ASSERT_EQ(expected_end_role, browser_accessibility_end->GetRole());

    AssertScrollIntoView(root_browser_accessibility,
                         browser_accessibility_start, browser_accessibility_end,
                         align_to_top);
  }

  // Run through ITextRangeProvider::ScrollIntoView top tests. It's assumed that
  // the browser has already loaded an HTML document's accessibility tree.
  // Assert the text range generated for an accessibility node is scrolled to be
  // flush with the top of the viewport.
  //   expected_start_role: the expected accessibility role of the text range
  //                        start node under test
  //   fstart:              the function to retrieve the accessibility text
  //                        range start node under test from the root
  //                        accessibility node
  //   fstart_arg:          an index argument for fstart
  //   expected_end_role:   the expected accessibility role of the text range
  //                        end node under test
  //   fend:                the function to retrieve the accessibility text
  //                        range end node under test from the root
  //                        accessibility node
  //   fend_arg:            an index argument for fend
  //   align_to_top:        true to test top viewport alignment, otherwise test
  //                        bottom viewport alignment
  void ScrollIntoViewBrowserTestTemplate(
      const ax::mojom::Role expected_start_role,
      BrowserAccessibility* (BrowserAccessibility::*fstart)(uint32_t) const,
      const uint32_t fstart_arg,
      const ax::mojom::Role expected_end_role,
      BrowserAccessibility* (BrowserAccessibility::*fend)(uint32_t) const,
      const uint32_t fend_arg,
      const bool align_to_top) {
    BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();

    BrowserAccessibility* browser_accessibility_start =
        (root_browser_accessibility->*fstart)(fstart_arg);
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    BrowserAccessibility* browser_accessibility_end =
        (root_browser_accessibility->*fend)(fend_arg);
    ASSERT_NE(nullptr, browser_accessibility_end);
    ASSERT_EQ(expected_end_role, browser_accessibility_end->GetRole());

    AssertScrollIntoView(root_browser_accessibility,
                         browser_accessibility_start, browser_accessibility_end,
                         align_to_top);
  }

  void ScrollIntoViewFromIframeBrowserTestTemplate(
      const ax::mojom::Role expected_start_role,
      BrowserAccessibility* (BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_end_role,
      BrowserAccessibility* (BrowserAccessibility::*fend)() const,
      const bool align_to_top) {
    BrowserAccessibility* root_browser_accessibility =
        GetRootAndAssertNonNull();
    BrowserAccessibility* leaf_iframe_browser_accessibility =
        root_browser_accessibility->InternalDeepestLastChild();
    ASSERT_NE(nullptr, leaf_iframe_browser_accessibility);
    ASSERT_EQ(ax::mojom::Role::kIframe,
              leaf_iframe_browser_accessibility->GetRole());

    AXTreeID iframe_tree_id = AXTreeID::FromString(
        leaf_iframe_browser_accessibility->GetStringAttribute(
            ax::mojom::StringAttribute::kChildTreeId));
    BrowserAccessibilityManager* iframe_browser_accessibility_manager =
        BrowserAccessibilityManager::FromID(iframe_tree_id);
    ASSERT_NE(nullptr, iframe_browser_accessibility_manager);
    BrowserAccessibility* root_iframe_browser_accessibility =
        iframe_browser_accessibility_manager->GetRoot();
    ASSERT_NE(nullptr, root_iframe_browser_accessibility);
    ASSERT_EQ(ax::mojom::Role::kRootWebArea,
              root_iframe_browser_accessibility->GetRole());

    BrowserAccessibility* browser_accessibility_start =
        (root_iframe_browser_accessibility->*fstart)();
    ASSERT_NE(nullptr, browser_accessibility_start);
    ASSERT_EQ(expected_start_role, browser_accessibility_start->GetRole());

    BrowserAccessibility* browser_accessibility_end =
        (root_iframe_browser_accessibility->*fend)();
    ASSERT_NE(nullptr, browser_accessibility_end);
    ASSERT_EQ(expected_end_role, browser_accessibility_end->GetRole());

    AssertScrollIntoView(root_iframe_browser_accessibility,
                         browser_accessibility_start, browser_accessibility_end,
                         align_to_top);
  }

  void AssertScrollIntoView(BrowserAccessibility* root_browser_accessibility,
                            BrowserAccessibility* browser_accessibility_start,
                            BrowserAccessibility* browser_accessibility_end,
                            const bool align_to_top) {
    ui::AXNodePosition::AXPositionInstance start =
        browser_accessibility_start->CreateTextPositionAt(0);
    ui::AXNodePosition::AXPositionInstance end =
        browser_accessibility_end->CreateTextPositionAt(0)
            ->CreatePositionAtEndOfAnchor();

    BrowserAccessibilityComWin* start_browser_accessibility_com_win =
        ToBrowserAccessibilityWin(browser_accessibility_start)->GetCOM();
    ASSERT_NE(nullptr, start_browser_accessibility_com_win);

    CComPtr<ITextRangeProvider> text_range_provider =
        ui::AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
            start_browser_accessibility_com_win, std::move(start),
            std::move(end));
    ASSERT_NE(nullptr, text_range_provider);

    gfx::Rect previous_range_bounds =
        align_to_top ? browser_accessibility_start->GetBoundsRect(
                           ui::AXCoordinateSystem::kFrame,
                           ui::AXClippingBehavior::kUnclipped)
                     : browser_accessibility_end->GetBoundsRect(
                           ui::AXCoordinateSystem::kFrame,
                           ui::AXClippingBehavior::kUnclipped);

    AccessibilityNotificationWaiter location_changed_waiter(
        GetWebContentsAndAssertNonNull(), ui::kAXModeComplete,
        ax::mojom::Event::kLocationChanged);
    ASSERT_HRESULT_SUCCEEDED(text_range_provider->ScrollIntoView(align_to_top));
    location_changed_waiter.WaitForNotification();

    gfx::Rect root_page_bounds = root_browser_accessibility->GetBoundsRect(
        ui::AXCoordinateSystem::kFrame, ui::AXClippingBehavior::kUnclipped);
    if (align_to_top) {
      gfx::Rect range_bounds = browser_accessibility_start->GetBoundsRect(
          ui::AXCoordinateSystem::kFrame, ui::AXClippingBehavior::kUnclipped);
      ASSERT_NE(previous_range_bounds.y(), range_bounds.y());
      ASSERT_NEAR(root_page_bounds.y(), range_bounds.y(), 1);
    } else {
      gfx::Rect range_bounds = browser_accessibility_end->GetBoundsRect(
          ui::AXCoordinateSystem::kFrame, ui::AXClippingBehavior::kUnclipped);
      gfx::Size viewport_size =
          gfx::Size(root_page_bounds.width(), root_page_bounds.height());
      ASSERT_NE(previous_range_bounds.y(), range_bounds.y());
      ASSERT_NEAR(root_page_bounds.y() + viewport_size.height(),
                  range_bounds.y() + range_bounds.height(), 1);
    }
  }

  void ScrollIntoViewTopBrowserTestTemplate(
      const ax::mojom::Role expected_role,
      BrowserAccessibility* (BrowserAccessibility::*f)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role, f, expected_role, f, true);
  }

  void ScrollIntoViewTopBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      BrowserAccessibility* (BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_role_end,
      BrowserAccessibility* (BrowserAccessibility::*fend)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart,
                                      expected_role_end, fend, true);
  }

  void ScrollIntoViewTopBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      BrowserAccessibility* (BrowserAccessibility::*fstart)(uint32_t) const,
      const uint32_t fstart_arg,
      const ax::mojom::Role expected_role_end,
      BrowserAccessibility* (BrowserAccessibility::*fend)(uint32_t) const,
      const uint32_t fend_arg) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart, fstart_arg,
                                      expected_role_end, fend, fend_arg, true);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role,
      BrowserAccessibility* (BrowserAccessibility::*f)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role, f, expected_role, f,
                                      false);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      BrowserAccessibility* (BrowserAccessibility::*fstart)() const,
      const ax::mojom::Role expected_role_end,
      BrowserAccessibility* (BrowserAccessibility::*fend)() const) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart,
                                      expected_role_end, fend, false);
  }

  void ScrollIntoViewBottomBrowserTestTemplate(
      const ax::mojom::Role expected_role_start,
      BrowserAccessibility* (BrowserAccessibility::*fstart)(uint32_t) const,
      const uint32_t fstart_arg,
      const ax::mojom::Role expected_role_end,
      BrowserAccessibility* (BrowserAccessibility::*fend)(uint32_t) const,
      const uint32_t fend_arg) {
    ScrollIntoViewBrowserTestTemplate(expected_role_start, fstart, fstart_arg,
                                      expected_role_end, fend, fend_arg, false);
  }
};

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       GetBoundingRectangles) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .break_word {
              width: 50px;
              word-wrap: break-word;
            }
          </style>
        </head>
        <body>
          <p class="break_word">AsdfAsdfAsdf</p>
        </body>
      </html>
  )HTML"));

  auto* node = FindNode(ax::mojom::Role::kStaticText, "AsdfAsdfAsdf");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"AsdfAsdfAsdf");

  base::win::ScopedSafearray rectangles;
  EXPECT_HRESULT_SUCCEEDED(
      text_range_provider->GetBoundingRectangles(rectangles.Receive()));

  // |view_offset| is necessary to account for differences in the shell
  // between platforms (e.g. title bar height) because the results of
  // |GetBoundingRectangles| are in screen coordinates.
  gfx::Vector2d view_offset =
      node->manager()->GetViewBounds().OffsetFromOrigin();
  std::vector<double> expected_values = {
      8 + view_offset.x(), 16 + view_offset.y(), 49, 17,
      8 + view_offset.x(), 34 + view_offset.y(), 44, 17};
  EXPECT_UIA_DOUBLE_SAFEARRAY_EQ(rectangles.Get(), expected_values);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopStaticText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomStaticText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopEmbeddedText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomEmbeddedText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopEmbeddedTextCrossNode) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomEmbeddedTextCrossNode) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/embedded-text.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopTable) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kTable, &BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kTable, &BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomTable) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kTable, &BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kTable, &BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopTableText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomTableText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/table.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopLinkText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewTopBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomLinkText) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopLinkContainer) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewTopBrowserTestTemplate(ax::mojom::Role::kGenericContainer,
                                       &BrowserAccessibility::PlatformGetChild,
                                       0, ax::mojom::Role::kGenericContainer,
                                       &BrowserAccessibility::PlatformGetChild,
                                       0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomLinkContainer) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/link.html");
  ScrollIntoViewBottomBrowserTestTemplate(
      ax::mojom::Role::kGenericContainer,
      &BrowserAccessibility::PlatformGetChild, 0,
      ax::mojom::Role::kGenericContainer,
      &BrowserAccessibility::PlatformGetChild, 0);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewTopTextFromIFrame) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/iframe-text.html");
  WaitForAccessibilityTreeToContainNodeWithName(
      shell()->web_contents(),
      "Game theory is \"the study of Mathematical model mathematical models of "
      "conflict and cooperation between intelligent rational decision-makers."
      "\"");
  ScrollIntoViewFromIframeBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild, true);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ScrollIntoViewBottomTextFromIFrame) {
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/scrolling/iframe-text.html");
  WaitForAccessibilityTreeToContainNodeWithName(
      shell()->web_contents(),
      "Game theory is \"the study of Mathematical model mathematical models of "
      "conflict and cooperation between intelligent rational decision-makers."
      "\"");
  ScrollIntoViewFromIframeBrowserTestTemplate(
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestFirstChild,
      ax::mojom::Role::kStaticText,
      &BrowserAccessibility::PlatformDeepestLastChild, false);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitFormat) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>plain 1</div><div>plain 2</div>
        <div style="font-style: italic">italic 1</div>
        <div style="font-style: italic">italic 2</div>
        <div style="font-weight: bold">bold 1</div>
        <div style="font-weight: bold">bold 2</div>
      </body>
      </html>)HTML");
  auto* node = FindNode(ax::mojom::Role::kStaticText, "plain 1");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"plain 1");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/ L"plain 1\nplain 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nitalic 1\nitalic 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/ L"plain 1\nplain 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nitalic 1\nitalic 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 5,
      /*expected_text*/
      L"plain 1\nplain 2\nitalic 1\nitalic 2\nbold 1\nbold 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -4,
      /*expected_text*/ L"",
      /*expected_count*/ -3);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/ L"plain 1\nplain 2",
      /*expected_count*/ 1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitFormatAllFormats) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>plain 1</div><div>plain 2</div>
        <div style="background-color: red">background-color 1</div>
        <div style="background-color: red">background-color 2</div>
        <div style="color: blue">color 1</div>
        <div style="color: blue">color 2</div>
        <div style="text-decoration: overline">overline 1</div>
        <div style="text-decoration: overline">overline 2</div>
        <div style="text-decoration: line-through">line-through 1</div>
        <div style="text-decoration: line-through">line-through 2</div>
        <div style="vertical-align:super">sup 1</div>
        <div style="vertical-align:super">sup 2</div>
        <div style="font-weight: bold">bold 1</div>
        <div style="font-weight: bold">bold 2</div>
        <div style="font-family: sans-serif">font-family 1</div>
        <div style="font-family: sans-serif">font-family 2</div>
      </body>
      </html>)HTML");
  auto* node = FindNode(ax::mojom::Role::kStaticText, "plain 1");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"plain 1");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/ L"plain 1\nplain 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/ L"plain 1\nplain 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\n",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ 2,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2\nfont-family 1\nfont-family 2",
      /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Format,
      /*count*/ -1,
      /*expected_text*/
      L"plain 1\nplain 2\nbackground-color 1\nbackground-color 2\ncolor "
      L"1\ncolor 2\noverline 1\noverline 2\nline-through 1\nline-through "
      L"2\nsup 1\nsup 2\nbold 1\nbold 2\n",
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       MoveEndpointByUnitParagraph) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <head><style>
        div {
          width: 100px;
        }
        code::before {
          content: "[";
        }
        code::after {
          content: "]";
        }
        b::before, i::after {
          width: 5px;
          height: 5px;
          content: "";
          display: block;
          background: black;
        }
      </style></head>
      <body>
        <div>start</div>
        <div>
          text with <code>:before</code>
          and <code>:after</code> content,
          then a <b>bold</b> element with a
          <code>block</code> before content
          then a <i>italic</i> element with
          a <code>block</code> after content
        </div>
        <div>end</div>
      </body>
      </html>)HTML");
  BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  ComPtr<ITextRangeProvider> text_range_provider;

  std::vector<base::string16> paragraphs = {
      L"start",
      L"text with [:before] and [:after]content, then a",
      L"bold element with a [block]before content then a italic",
      L"element with a [block] after content",
      L"end",
  };

  // FORWARD NAVIGATION
  GetTextRangeProviderFromTextNode(text_range_provider, start_node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"start");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -2,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + paragraphs[3]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[3] + L"\n" + paragraphs[4]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 2,
      /*expected_text*/ L"",
      /*expected_count*/ 1);

  // REVERSE NAVIGATION
  GetTextRangeProviderFromTextNode(text_range_provider, end_node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"end");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 2,
      /*expected_text*/ L"",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[4].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[3] + L"\n" + paragraphs[4]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + paragraphs[3]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ 0);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -2,
      /*expected_text*/ L"",
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(
    AXPlatformNodeTextRangeProviderWinBrowserTest,
    MoveEndpointByUnitParagraphCollapseTrailingLineBreakingObjects) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>start</div>
        <div>
          <div>some text</div>
          <div></div>
          <br>
          <span><br><div><br></div></span>
          <div>more text</div>
        </div>
        <div>end</div>
      </body>
      </html>)HTML");
  BrowserAccessibility* start_node =
      FindNode(ax::mojom::Role::kStaticText, "start");
  ASSERT_NE(nullptr, start_node);
  BrowserAccessibility* end_node =
      FindNode(ax::mojom::Role::kStaticText, "end");
  ASSERT_NE(nullptr, end_node);

  ComPtr<ITextRangeProvider> text_range_provider;

  std::vector<base::string16> paragraphs = {
      L"start",
      L"some text\n\n\n",
      L"more text",
      L"end",
  };

  // FORWARD NAVIGATION
  GetTextRangeProviderFromTextNode(text_range_provider, start_node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"start");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ (paragraphs[2] + L"\n" + paragraphs[3]).c_str(),
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ 1,
      /*expected_text*/ paragraphs[3].c_str(),
      /*expected_count*/ 1);

  // REVERSE NAVIGATION
  GetTextRangeProviderFromTextNode(text_range_provider, end_node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"end");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[2] + L"\n" + paragraphs[3]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[2].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[1] + paragraphs[2]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[1].c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ (paragraphs[0] + L"\n" + paragraphs[1]).c_str(),
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Paragraph,
      /*count*/ -1,
      /*expected_text*/ paragraphs[0].c_str(),
      /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       IFrameTraversal) {
  LoadInitialAccessibilityTreeFromUrl(embedded_test_server()->GetURL(
      "/accessibility/html/iframe-cross-process.html"));

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  auto* node = FindNode(ax::mojom::Role::kStaticText, "After frame");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"After frame");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
      /*count*/ -1,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
      /*count*/ -2,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ -2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ -3,
                                   /*expected_text*/ L"Text in",
                                   /*expected_count*/ -3);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ 2,
                                   /*expected_text*/ L"Text in iframe\nAfter",
                                   /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Document,
      /*count*/ 1,
      /*expected_text*/ L"",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ -17,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -17);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Line,
                                   /*count*/ -1,
                                   /*expected_text*/ L"iframe",
                                   /*expected_count*/ -1);

  text_range_provider->ExpandToEnclosingUnit(TextUnit_Line);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"Text in iframe\n");

  text_range_provider->ExpandToEnclosingUnit(TextUnit_Document);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          L"Before frame\nText in iframe\nAfter frame");

  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Word,
                  /*count*/ 2,
                  /*expected_text*/ L"Text",
                  /*expected_count*/ 2);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Word,
                  /*count*/ -1,
                  /*expected_text*/ L"frame",
                  /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ 1,
      /*expected_text*/ L"frame\nT",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 6,
                  /*expected_text*/ L"e",
                  /*expected_count*/ 6);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ 19,
                  /*expected_text*/ L"f",
                  /*expected_count*/ 19);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Character,
                  /*count*/ -7,
                  /*expected_text*/ L"e",
                  /*expected_count*/ -7);
  EXPECT_UIA_MOVE(text_range_provider, TextUnit_Line,
                  /*count*/ 1,
                  /*expected_text*/ L"After frame",
                  /*expected_count*/ 1);

  text_range_provider->ExpandToEnclosingUnit(TextUnit_Document);
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider,
                          L"Before frame\nText in iframe\nAfter frame");
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       OutOfProcessIFrameTraversal) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/iframe-cross-process.html"));
  LoadInitialAccessibilityTreeFromUrl(main_url);

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  // Navigate oopif to URL.
  FrameTreeNode* iframe_node = root->child_at(0);
  GURL iframe_url(embedded_test_server()->GetURL(
      "b.com", "/accessibility/html/frame/static_text.html"));
  WebContentsImpl* iframe_web_contents =
      WebContentsImpl::FromFrameTreeNode(iframe_node);
  DCHECK(iframe_web_contents);
  {
    AccessibilityNotificationWaiter waiter(iframe_web_contents,
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLoadComplete);
    NavigateFrameToURL(iframe_node, iframe_url);
    waiter.WaitForNotification();
  }

  SynchronizeThreads();
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Text in iframe");

  WaitForHitTestDataOrChildSurfaceReady(iframe_node->current_frame_host());
  FrameTreeVisualizer visualizer;
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      visualizer.DepictFrameTree(root));

  auto* node = FindNode(ax::mojom::Role::kStaticText, "After frame");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"After frame");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
      /*count*/ -1,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Word,
      /*count*/ -2,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ -2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ -3,
                                   /*expected_text*/ L"Text in",
                                   /*expected_count*/ -3);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Word,
                                   /*count*/ 2,
                                   /*expected_text*/ L"Text in iframe\nAfter",
                                   /*expected_count*/ 2);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Line,
      /*count*/ 1,
      /*expected_text*/ L"Text in iframe\nAfter frame",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Document,
      /*count*/ 1,
      /*expected_text*/ L"",
      /*expected_count*/ 1);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ -17,
      /*expected_text*/ L"iframe\nAfter frame",
      /*expected_count*/ -17);
  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(text_range_provider,
                                   TextPatternRangeEndpoint_End, TextUnit_Line,
                                   /*count*/ -1,
                                   /*expected_text*/ L"iframe",
                                   /*expected_count*/ -1);
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextRangeProviderWinBrowserTest,
                       ExpandToEnclosingFormat) {
  LoadInitialAccessibilityTreeFromHtml(
      R"HTML(<!DOCTYPE html>
      <html>
      <body>
        <div>plain</div>
        <div>text</div>
        <div style="font-style: italic">italic<div>
        <div style="font-style: italic">text<div>
      </body>
      </html>)HTML");

  auto* node = FindNode(ax::mojom::Role::kStaticText, "plain");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->PlatformIsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextRangeProvider> text_range_provider;
  GetTextRangeProviderFromTextNode(text_range_provider, node);
  ASSERT_NE(nullptr, text_range_provider.Get());
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"plain");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 3,
      /*expected_text*/ L"in",
      /*expected_count*/ 3);

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Format));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"plain\ntext\n");

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_End, TextUnit_Character,
      /*count*/ 3,
      /*expected_text*/ L"plain\ntext\nita",
      /*expected_count*/ 3);

  EXPECT_UIA_MOVE_ENDPOINT_BY_UNIT(
      text_range_provider, TextPatternRangeEndpoint_Start, TextUnit_Character,
      /*count*/ 10,
      /*expected_text*/ L"ta",
      /*expected_count*/ 10);

  ASSERT_HRESULT_SUCCEEDED(
      text_range_provider->ExpandToEnclosingUnit(TextUnit_Format));
  EXPECT_UIA_TEXTRANGE_EQ(text_range_provider, L"italic\ntext");
}
}  // namespace content
