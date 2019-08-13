// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_page.h"

#include "base/logging.h"
#include "base/test/gtest_util.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_utils.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#endif

namespace chrome_pdf {

namespace {

bool IsValidLinkForTesting(const std::string& url) {
  return !url.empty();
}

TEST(PDFiumPageTest, ToPDFiumRotation) {
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kOriginal), 0);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise90), 1);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise180), 2);
  EXPECT_EQ(ToPDFiumRotation(PageOrientation::kClockwise270), 3);
}

TEST(PDFiumPageDeathTest, ToPDFiumRotation) {
  PageOrientation invalid_orientation = static_cast<PageOrientation>(-1);
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH(ToPDFiumRotation(invalid_orientation));
#else
  EXPECT_EQ(ToPDFiumRotation(invalid_orientation), 0);
#endif
}

}  // namespace

class PDFiumPageLinkTest : public PDFiumTestBase {
 public:
  PDFiumPageLinkTest() {
    PDFiumPage::SetIsValidLinkFunctionForTesting(&IsValidLinkForTesting);
  }

  ~PDFiumPageLinkTest() override {
    PDFiumPage::SetIsValidLinkFunctionForTesting(nullptr);
  }

  const std::vector<PDFiumPage::Link>& GetLinks(PDFiumEngine* engine,
                                                int page_index) {
    PDFiumPage* page = GetPDFiumPageForTest(engine, page_index);
    DCHECK(page);
    page->CalculateLinks();
    return page->links_;
  }

  DISALLOW_COPY_AND_ASSIGN(PDFiumPageLinkTest);
};

TEST_F(PDFiumPageLinkTest, TestLinkGeneration) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("weblinks.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  bool is_chromeos = false;
#if defined(OS_CHROMEOS)
  is_chromeos = base::SysInfo::IsRunningOnChromeOS();
#endif

  const std::vector<PDFiumPage::Link>& links = GetLinks(engine.get(), 0);
  ASSERT_EQ(2u, links.size());

  const PDFiumPage::Link& link = links[0];
  EXPECT_EQ("http://yahoo.com", link.url);
  EXPECT_EQ(7, link.start_char_index);
  EXPECT_EQ(16, link.char_count);
  ASSERT_EQ(1u, link.bounding_rects.size());
  if (is_chromeos) {
    CompareRect({75, 192, 110, 15}, link.bounding_rects[0]);
  } else {
    CompareRect({75, 191, 110, 16}, link.bounding_rects[0]);
  }

  const PDFiumPage::Link& second_link = links[1];
  EXPECT_EQ("http://bing.com", second_link.url);
  EXPECT_EQ(52, second_link.start_char_index);
  EXPECT_EQ(15, second_link.char_count);
  ASSERT_EQ(1u, second_link.bounding_rects.size());
  if (is_chromeos) {
    CompareRect({131, 120, 138, 22}, second_link.bounding_rects[0]);
  } else {
    CompareRect({131, 121, 138, 20}, second_link.bounding_rects[0]);
  }
}

}  // namespace chrome_pdf