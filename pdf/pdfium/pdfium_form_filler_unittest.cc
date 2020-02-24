// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "ppapi/c/pp_point.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/pdfium/public/fpdf_annot.h"

using testing::InSequence;

namespace chrome_pdf {

namespace {

class FormFillerTestClient : public TestClient {
 public:
  FormFillerTestClient() = default;
  ~FormFillerTestClient() override = default;
  FormFillerTestClient(const FormFillerTestClient&) = delete;
  FormFillerTestClient& operator=(const FormFillerTestClient&) = delete;

  // Mock PDFEngine::Client methods.
  MOCK_METHOD1(ScrollToX, void(int));
  MOCK_METHOD2(ScrollToY, void(int, bool));
};

}  // namespace

class FormFillerTest : public PDFiumTestBase {
 public:
  FormFillerTest() = default;
  ~FormFillerTest() override = default;
  FormFillerTest(const FormFillerTest&) = delete;
  FormFillerTest& operator=(const FormFillerTest&) = delete;

  void TriggerFormFocusChange(PDFiumEngine* engine,
                              FPDF_ANNOTATION annot,
                              int page_index) {
    ASSERT_TRUE(engine);
    engine->form_filler_.Form_OnFocusChange(&engine->form_filler_, annot,
                                            page_index);
  }
};

TEST_F(FormFillerTest, FormOnFocusChange) {
  struct {
    // Initial scroll position of the document.
    PP_Point initial_position;
    // Page number on which the annotation is present.
    int page_index;
    // The index of test annotation on page_index.
    int annot_index;
    // The scroll position to bring the annotation into view. (0,0) if the
    // annotation is already in view.
    PP_Point final_scroll_position;
  } static constexpr test_cases[] = {
      {{0, 0}, 0, 0, {242, 746}},   {{0, 0}, 0, 1, {510, 478}},
      {{242, 40}, 0, 0, {0, 746}},  {{60, 758}, 0, 0, {242, 0}},
      {{242, 758}, 0, 0, {0, 0}},   {{242, 768}, 0, 0, {0, 746}},
      {{274, 758}, 0, 0, {242, 0}}, {{60, 40}, 1, 0, {242, 1816}}};

  FormFillerTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());
  engine->PluginSizeUpdated(pp::Size(60, 40));

  {
    InSequence sequence;

    for (const auto& test_case : test_cases) {
      if (test_case.final_scroll_position.y != 0) {
        EXPECT_CALL(client,
                    ScrollToY(test_case.final_scroll_position.y, false));
      }
      if (test_case.final_scroll_position.x != 0)
        EXPECT_CALL(client, ScrollToX(test_case.final_scroll_position.x));
    }
  }

  for (const auto& test_case : test_cases) {
    // Setting up the initial scroll positions.
    engine->ScrolledToXPosition(test_case.initial_position.x);
    engine->ScrolledToYPosition(test_case.initial_position.y);

    PDFiumPage* page = GetPDFiumPageForTest(engine.get(), test_case.page_index);
    ScopedFPDFAnnotation annot(
        FPDFPage_GetAnnot(page->GetPage(), test_case.annot_index));
    ASSERT_TRUE(annot);
    TriggerFormFocusChange(engine.get(), annot.get(), test_case.page_index);
  }
}

}  // namespace chrome_pdf