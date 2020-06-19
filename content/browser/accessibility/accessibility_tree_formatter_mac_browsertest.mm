// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace content {

namespace {

class AccessibilityTreeFormatterMacBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityTreeFormatterMacBrowserTest() {}
  ~AccessibilityTreeFormatterMacBrowserTest() override {}

  // Checks the formatted accessible tree for the given data URL.
  void TestAndCheck(const char* url,
                    const std::vector<const char*>& filters,
                    const char* expected) const;

 protected:
  BrowserAccessibilityManager* GetManager() const {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }
};

void AccessibilityTreeFormatterMacBrowserTest::TestAndCheck(
    const char* url,
    const std::vector<const char*>& filters,
    const char* expected) const {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url)));
  waiter.WaitForNotification();

  // Set property filters
  std::unique_ptr<AccessibilityTreeFormatter> formatter =
      AccessibilityTreeFormatter::Create();

  std::vector<AccessibilityTreeFormatter::PropertyFilter> property_filters;

  for (const char* filter : filters) {
    property_filters.push_back(AccessibilityTreeFormatter::PropertyFilter(
        base::UTF8ToUTF16(filter),
        AccessibilityTreeFormatter::PropertyFilter::ALLOW_EMPTY));
  }

  formatter->AddDefaultFilters(&property_filters);
  formatter->SetPropertyFilters(property_filters);

  // Format the tree
  BrowserAccessibility* root = GetManager()->GetRoot();
  CHECK(root);

  base::string16 contents;
  formatter->FormatAccessibilityTreeForTesting(root, &contents);

  auto got = base::UTF16ToUTF8(contents);
  EXPECT_EQ(got, expected);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       DefaultAttributes) {
  TestAndCheck(R"~~(data:text/html,
                    <input aria-label='input'>)~~",
               {},
               R"~~(AXWebArea
++AXGroup
++++AXTextField AXDescription='input'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       LineIndexFilter) {
  TestAndCheck(R"~~(data:text/html,
                    <input class='input_at_3rd_line'>
                    <input class='input_at_4th_line'>
                    <input class='input_at_5th_line'>)~~",
               {":3,:5;AXDOMClassList=*"}, R"~~(AXWebArea
++AXGroup
++++AXTextField AXDOMClassList=['input_at_3rd_line']
++++AXTextField
++++AXTextField AXDOMClassList=['input_at_5th_line']
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Serialize_AXTextMarker) {
  TestAndCheck(R"~~(data:text/html,
                    <p>Paragraph</p>)~~",
               {":3;AXStartTextMarker=*"}, R"~~(AXWebArea
++AXGroup
++++AXStaticText AXStartTextMarker={:1, 0, down} AXValue='Paragraph'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       Serialize_AXTextMarkerRange) {
  TestAndCheck(R"~~(data:text/html,
                    <p id='p'>Paragraph</p>
                    <script>
                      window.getSelection().selectAllChildren(document.getElementById('p'));
                    </script>)~~",
               {":3;AXSelectedTextMarkerRange=*"}, R"~~(AXWebArea
++AXGroup
++++AXStaticText AXSelectedTextMarkerRange={anchor: {:3, 0, down}, focus: {:2, -1, down}} AXValue='Paragraph'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_Int) {
  TestAndCheck(R"~~(data:text/html,
                    <p contentEditable='true'>Text</p>)~~",
               {":2;AXLineForIndex(0)=*"}, R"~~(AXWebArea
++AXTextArea AXLineForIndex(0)=0 AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_Int_WrongArgs) {
  TestAndCheck(R"~~(data:text/html,
                    <p contentEditable='true'>Text</p>)~~",
               {":2;AXLineForIndex(NaN)=*"}, R"~~(AXWebArea
++AXTextArea AXLineForIndex(NaN)=ERROR:FAILED_TO_PARSE_ARGS AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_IntArray) {
  TestAndCheck(R"~~(data:text/html,
                    <table role="grid"><tr><td>CELL</td></tr></table>)~~",
               {"AXCellForColumnAndRow([0, 0])=*"}, R"~~(AXWebArea
++AXTable AXCellForColumnAndRow([0, 0])=:4
++++AXRow
++++++AXCell
++++++++AXStaticText AXValue='CELL'
++++AXColumn
++++++AXCell
++++++++AXStaticText AXValue='CELL'
++++AXGroup
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_IntArray_WrongArgs) {
  TestAndCheck(R"~~(data:text/html,
                    <table role="grid"><tr><td>CELL</td></tr></table>)~~",
               {"AXCellForColumnAndRow(0, 0)=*"}, R"~~(AXWebArea
++AXTable AXCellForColumnAndRow(0, 0)=ERROR:FAILED_TO_PARSE_ARGS
++++AXRow
++++++AXCell
++++++++AXStaticText AXValue='CELL'
++++AXColumn
++++++AXCell
++++++++AXStaticText AXValue='CELL'
++++AXGroup
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_IntArray_NilValue) {
  TestAndCheck(R"~~(data:text/html,
                    <table role="grid"></table>)~~",
               {"AXCellForColumnAndRow([0, 0])=*"}, R"~~(AXWebArea
++AXTable AXCellForColumnAndRow([0, 0])=NULL
++++AXGroup
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_NSRange) {
  TestAndCheck(R"~~(data:text/html,
                    <p contentEditable='true'>Text</p>)~~",
               {":2;AXStringForRange({loc: 1, len: 2})=*"}, R"~~(AXWebArea
++AXTextArea AXStringForRange({loc: 1, len: 2})='ex' AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

IN_PROC_BROWSER_TEST_F(AccessibilityTreeFormatterMacBrowserTest,
                       ParameterizedAttributes_NSRAnge_WrongArgs) {
  TestAndCheck(R"~~(data:text/html,
                    <p contentEditable='true'>Text</p>)~~",
               {":2;AXStringForRange({loco: 1, leno: 2})=*"}, R"~~(AXWebArea
++AXTextArea AXStringForRange({loco: 1, leno: 2})=ERROR:FAILED_TO_PARSE_ARGS AXValue='Text'
++++AXStaticText AXValue='Text'
)~~");
}

}  // namespace content
