// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility.h"

#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_utils.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

using AccessibilityTest = PDFiumTestBase;

float GetExpectedBoundsWidth(bool is_chromeos, size_t i, float expected) {
  return (is_chromeos && i == 0) ? 85.333336f : expected;
}

double GetExpectedCharWidth(bool is_chromeos, size_t i, double expected) {
  if (is_chromeos) {
    if (i == 25)
      return 13.333343;
    if (i == 26)
      return 6.666656;
  }
  return expected;
}

// NOTE: This test is sensitive to font metrics from the underlying platform.
// If changes to fonts on the system or to font code like FreeType cause this
// test to fail, please feel free to rebase the test expectations here, or
// update the GetExpected... functions above. If that becomes too much of a
// burden, consider changing the checks to just make sure the font metrics look
// sane.
TEST_F(AccessibilityTest, GetAccessibilityPage) {
  static constexpr size_t kExpectedTextRunCount = 2;
  struct {
    uint32_t len;
    double font_size;
    float bounds_x;
    float bounds_y;
    float bounds_w;
    float bounds_h;
  } static constexpr kExpectedTextRuns[] = {
      {15, 12, 26.666666f, 189.333328f, 84.000008f, 13.333344f},
      {15, 16, 28.000000f, 117.333334f, 152.000000f, 19.999992f},
  };
  static_assert(base::size(kExpectedTextRuns) == kExpectedTextRunCount,
                "Bad test expectation count");

  static constexpr size_t kExpectedCharCount = 30;
  static constexpr PP_PrivateAccessibilityCharInfo kExpectedChars[] = {
      {'H', 12}, {'e', 6.6666}, {'l', 5.3333}, {'l', 4},      {'o', 8},
      {',', 4},  {' ', 4},      {'w', 12},     {'o', 6.6666}, {'r', 6.6666},
      {'l', 4},  {'d', 9.3333}, {'!', 4},      {'\r', 0},     {'\n', 0},
      {'G', 16}, {'o', 12},     {'o', 12},     {'d', 12},     {'b', 10.6666},
      {'y', 12}, {'e', 12},     {',', 4},      {' ', 6.6666}, {'w', 16},
      {'o', 12}, {'r', 8},      {'l', 4},      {'d', 12},     {'!', 2.6666},
  };
  static_assert(base::size(kExpectedChars) == kExpectedCharCount,
                "Bad test expectation count");

  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());
  PP_PrivateAccessibilityPageInfo page_info;
  std::vector<PP_PrivateAccessibilityTextRunInfo> text_runs;
  std::vector<PP_PrivateAccessibilityCharInfo> chars;
  std::vector<PP_PrivateAccessibilityLinkInfo> links;
  std::vector<PP_PrivateAccessibilityImageInfo> images;
  ASSERT_TRUE(GetAccessibilityInfo(engine.get(), 0, &page_info, &text_runs,
                                   &chars, &links, &images));
  EXPECT_EQ(0u, page_info.page_index);
  EXPECT_EQ(5, page_info.bounds.point.x);
  EXPECT_EQ(3, page_info.bounds.point.y);
  EXPECT_EQ(266, page_info.bounds.size.width);
  EXPECT_EQ(266, page_info.bounds.size.height);
  EXPECT_EQ(text_runs.size(), page_info.text_run_count);
  EXPECT_EQ(chars.size(), page_info.char_count);
  // TODO(crbug.com/981448): Test the contents of |links| and |images| when
  // populated.
  EXPECT_EQ(links.size(), page_info.link_count);
  EXPECT_EQ(images.size(), page_info.image_count);

  bool is_chromeos = IsRunningOnChromeOS();

  ASSERT_EQ(kExpectedTextRunCount, text_runs.size());
  for (size_t i = 0; i < kExpectedTextRunCount; ++i) {
    const auto& expected = kExpectedTextRuns[i];
    EXPECT_EQ(expected.len, text_runs[i].len) << i;
    EXPECT_DOUBLE_EQ(expected.font_size, text_runs[i].font_size) << i;
    EXPECT_FLOAT_EQ(expected.bounds_x, text_runs[i].bounds.point.x) << i;
    EXPECT_FLOAT_EQ(expected.bounds_y, text_runs[i].bounds.point.y) << i;
    float expected_bounds_w =
        GetExpectedBoundsWidth(is_chromeos, i, expected.bounds_w);
    EXPECT_FLOAT_EQ(expected_bounds_w, text_runs[i].bounds.size.width) << i;
    EXPECT_FLOAT_EQ(expected.bounds_h, text_runs[i].bounds.size.height) << i;
    EXPECT_EQ(PP_PRIVATEDIRECTION_LTR, text_runs[i].direction);
  }

  ASSERT_EQ(kExpectedCharCount, chars.size());
  for (size_t i = 0; i < kExpectedCharCount; ++i) {
    const auto& expected = kExpectedChars[i];
    EXPECT_EQ(expected.unicode_character, chars[i].unicode_character) << i;
    double expected_char_width =
        GetExpectedCharWidth(is_chromeos, i, expected.char_width);
    EXPECT_NEAR(expected_char_width, chars[i].char_width, 0.001) << i;
  }
}

TEST_F(AccessibilityTest, GetUnderlyingTextRangeForRect) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage* page = GetPDFiumPageForTest(engine.get(), 0);
  ASSERT_TRUE(page);

  // The test rect spans across [0, 4] char indices.
  int start_index = -1;
  int char_count = 0;
  EXPECT_TRUE(page->GetUnderlyingTextRangeForRect(
      pp::FloatRect(20.0f, 50.0f, 26.0f, 8.0f), &start_index, &char_count));
  EXPECT_EQ(start_index, 0);
  EXPECT_EQ(char_count, 5);

  // The input rectangle is spanning across multiple lines.
  // GetUnderlyingTextRangeForRect() should return only the char indices
  // of first line.
  start_index = -1;
  char_count = 0;
  EXPECT_TRUE(page->GetUnderlyingTextRangeForRect(
      pp::FloatRect(20.0f, 0.0f, 26.0f, 58.0f), &start_index, &char_count));
  EXPECT_EQ(start_index, 0);
  EXPECT_EQ(char_count, 5);

  // There is no text below this rectangle. So, GetUnderlyingTextRangeForRect()
  // will return false and not change the dummy values set here.
  start_index = -9;
  char_count = -10;
  EXPECT_FALSE(page->GetUnderlyingTextRangeForRect(
      pp::FloatRect(10.0f, 10.0f, 0.0f, 0.0f), &start_index, &char_count));
  EXPECT_EQ(start_index, -9);
  EXPECT_EQ(char_count, -10);
}

// This class overrides TestClient to record points received when a scroll
// call is made by tests.
class ScrollEnabledTestClient : public TestClient {
 public:
  ScrollEnabledTestClient() = default;
  ~ScrollEnabledTestClient() override = default;

  // Records the point received in a ScrollBy action request from tests.
  void ScrollBy(const pp::Point& point) override { received_point_ = point; }

  // Returns the point received in a ScrollBy action for validation in tests.
  const pp::Point& GetScrollRequestPoints() const { return received_point_; }

 private:
  pp::Point received_point_;
};

TEST_F(AccessibilityTest, TestScrollIntoViewActionHandling) {
  // This test checks that accessibility scroll action is passed
  // on to the ScrollEnabledTestClient implementation.
  ScrollEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);
  engine->PluginSizeUpdated({400, 400});
  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_MAKE_VISIBLE;
  action_data.target_rect = {{120, 0}, {10, 10}};

  // Horizontal and Vertical scroll alignment of none should not scroll.
  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({0, 0}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({120, 0}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({120, -400}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({-280, 0}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({-280, -400}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({-80, -200}, client.GetScrollRequestPoints());

  // Simulate a 150% zoom update in the PDFiumEngine.
  engine->PluginSizeUpdated({600, 600});

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_NONE;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({0, 0}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({120, 0}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_LEFT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({120, -600}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_TOP;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({-480, 0}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_BOTTOM;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({-480, -600}, client.GetScrollRequestPoints());

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CENTER;
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({-180, -300}, client.GetScrollRequestPoints());
}

TEST_F(AccessibilityTest, TestScrollToNearestEdge) {
  ScrollEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);
  engine->PluginSizeUpdated({400, 400});
  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_MAKE_VISIBLE;

  action_data.horizontal_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE;
  action_data.vertical_scroll_alignment =
      PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE;
  // Point which is in the middle of the viewport.
  action_data.target_rect = {{200, 200}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({200, 200}, client.GetScrollRequestPoints());

  // Point which is near the top left of the viewport.
  action_data.target_rect = {{199, 199}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({199, 199}, client.GetScrollRequestPoints());

  // Point which is near the top right of the viewport
  action_data.target_rect = {{201, 199}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({-199, 199}, client.GetScrollRequestPoints());

  // Point which is near the bottom left of the viewport.
  action_data.target_rect = {{199, 201}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({199, -199}, client.GetScrollRequestPoints());

  // Point which is near the bottom right of the viewport
  action_data.target_rect = {{201, 201}, {10, 10}};
  engine->HandleAccessibilityAction(action_data);
  ComparePoint({-199, -199}, client.GetScrollRequestPoints());
}

// This class is required to just override the NavigateTo
// functionality for testing in a specific way. It will
// keep the TestClient class clean for extension by others.
class NavigationEnabledTestClient : public TestClient {
 public:
  NavigationEnabledTestClient() = default;
  ~NavigationEnabledTestClient() override = default;

  void NavigateTo(const std::string& url,
                  WindowOpenDisposition disposition) override {
    url_ = url;
    disposition_ = disposition;
  }

  void ScrollToY(int y_in_screen_coords, bool compensate_for_toolbar) override {
    y_scroll_offset_ = y_in_screen_coords;
    compensate_for_toolbar_ = compensate_for_toolbar;
  }

  const std::string& url() const { return url_; }
  WindowOpenDisposition disposition() const { return disposition_; }
  int y_scroll_offset() const { return y_scroll_offset_; }
  bool compensate_for_toolbar() const { return compensate_for_toolbar_; }

 private:
  std::string url_;
  WindowOpenDisposition disposition_ = WindowOpenDisposition::UNKNOWN;
  int y_scroll_offset_ = 0;
  bool compensate_for_toolbar_ = false;
};

TEST_F(AccessibilityTest, TestWebLinkClickActionHandling) {
  NavigationEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);

  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_DO_DEFAULT_ACTION;
  action_data.page_index = 0;
  action_data.link_index = 0;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ("http://yahoo.com", client.url());
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB, client.disposition());
}

TEST_F(AccessibilityTest, TestInternalLinkClickActionHandling) {
  NavigationEnabledTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  PP_PdfAccessibilityActionData action_data;
  action_data.action = PP_PdfAccessibilityAction::PP_PDF_DO_DEFAULT_ACTION;
  action_data.page_index = 0;
  action_data.link_index = 1;
  engine->HandleAccessibilityAction(action_data);
  EXPECT_EQ(1159, client.y_scroll_offset());
  EXPECT_TRUE(client.compensate_for_toolbar());
  EXPECT_TRUE(client.url().empty());
}

}  // namespace chrome_pdf
