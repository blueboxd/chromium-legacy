// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_TEST_BASE_H_
#define PDF_PDFIUM_PDFIUM_TEST_BASE_H_

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

class PDFiumEngine;
class PDFiumPage;
class TestClient;
class TestDocumentLoader;

class PDFiumTestBase : public testing::Test {
 public:
  PDFiumTestBase();
  PDFiumTestBase(const PDFiumTestBase&) = delete;
  PDFiumTestBase& operator=(const PDFiumTestBase&) = delete;
  ~PDFiumTestBase() override;

  // Returns true when actually running in a Chrome OS environment.
  static bool IsRunningOnChromeOS();

 protected:
  // Result of calling InitializeEngineWithoutLoading().
  struct InitializeEngineResult {
    InitializeEngineResult();
    InitializeEngineResult(InitializeEngineResult&& other) noexcept;
    InitializeEngineResult& operator=(InitializeEngineResult&& other) noexcept;
    ~InitializeEngineResult();

    // Initialized engine.
    std::unique_ptr<PDFiumEngine> engine;

    // Corresponding test document loader.
    TestDocumentLoader* document_loader;
  };

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Initializes a PDFiumEngine for use in testing with |client|. Loads a PDF
  // named |pdf_name|. See TestDocumentLoader for more info about |pdf_name|.
  std::unique_ptr<PDFiumEngine> InitializeEngine(
      TestClient* client,
      const base::FilePath::CharType* pdf_name);

  // Initializes a PDFiumEngine as with InitializeEngine(), but defers loading
  // until the test calls SimulateLoadData() on the returned TestDocumentLoader.
  InitializeEngineResult InitializeEngineWithoutLoading(
      TestClient* client,
      const base::FilePath::CharType* pdf_name);

  // Returns the PDFiumPage for the page index
  PDFiumPage* GetPDFiumPageForTest(PDFiumEngine* engine, size_t page_index);

 private:
  void InitializePDFium();
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_TEST_BASE_H_
