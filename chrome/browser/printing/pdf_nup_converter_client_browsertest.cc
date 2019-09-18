// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/printing/pdf_nup_converter_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "pdf/pdf.h"

namespace printing {

namespace {

void ResultCallbackImpl(mojom::PdfNupConverter::Status* status_out,
                        base::ReadOnlySharedMemoryRegion* nup_pdf_region_out,
                        bool* called,
                        base::OnceClosure quit_closure,
                        mojom::PdfNupConverter::Status status_in,
                        base::ReadOnlySharedMemoryRegion nup_pdf_region_in) {
  *status_out = status_in;
  *nup_pdf_region_out = std::move(nup_pdf_region_in);
  *called = true;
  std::move(quit_closure).Run();
}

base::FilePath GetTestDir() {
  base::FilePath dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &dir);
  if (!dir.empty())
    dir = dir.AppendASCII("printing");
  return dir;
}

base::MappedReadOnlyRegion GetPdfRegion(const char* file_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::MappedReadOnlyRegion pdf_region;

  base::FilePath test_data_dir = GetTestDir();
  if (test_data_dir.empty())
    return pdf_region;

  base::FilePath pdf_file = test_data_dir.AppendASCII(file_name);
  std::string pdf_str;
  if (!base::ReadFileToString(pdf_file, &pdf_str) || pdf_str.empty())
    return pdf_region;

  pdf_region = base::ReadOnlySharedMemoryRegion::Create(pdf_str.size());
  if (!pdf_region.IsValid())
    return pdf_region;

  memcpy(pdf_region.mapping.memory(), pdf_str.data(), pdf_str.size());
  return pdf_region;
}

base::MappedReadOnlyRegion GetBadDataRegion() {
  static const char kBadData[] = "BADDATA";
  base::MappedReadOnlyRegion pdf_region =
      base::ReadOnlySharedMemoryRegion::Create(base::size(kBadData));
  if (!pdf_region.IsValid())
    return pdf_region;

  memcpy(pdf_region.mapping.memory(), kBadData, base::size(kBadData));
  return pdf_region;
}

}  // namespace

class PdfNupConverterClientBrowserTest : public InProcessBrowserTest {
 public:
  PdfNupConverterClientBrowserTest() = default;
  ~PdfNupConverterClientBrowserTest() override = default;

  base::Optional<mojom::PdfNupConverter::Status> Convert(
      base::ReadOnlySharedMemoryRegion pdf_region,
      int pages_per_sheet,
      base::ReadOnlySharedMemoryRegion* out_nup_pdf_region) {
    mojom::PdfNupConverter::Status status;
    base::ReadOnlySharedMemoryRegion nup_pdf_region;
    bool called = false;
    auto converter = std::make_unique<PdfNupConverterClient>(
        browser()->tab_strip_model()->GetActiveWebContents());

    {
      base::RunLoop run_loop;
      converter->DoNupPdfDocumentConvert(
          /*document_cookie=*/8, pages_per_sheet,
          /*page_size=*/gfx::Size(2550, 3300),
          /*printable_area=*/gfx::Rect(2550, 3300), std::move(pdf_region),
          base::BindOnce(&ResultCallbackImpl, &status, &nup_pdf_region, &called,
                         run_loop.QuitClosure()));
      run_loop.Run();
    }

    if (!called)
      return base::nullopt;

    *out_nup_pdf_region = std::move(nup_pdf_region);
    return status;
  }
};

IN_PROC_BROWSER_TEST_F(PdfNupConverterClientBrowserTest,
                       DocumentConvert2UpSuccess) {
  base::MappedReadOnlyRegion pdf_region =
      GetPdfRegion("pdf_converter_basic.pdf");
  ASSERT_TRUE(pdf_region.IsValid());

  // Make sure pdf_converter_basic.pdf has 3 pages.
  int num_pages;
  double max_width_in_points;
  ASSERT_TRUE(
      chrome_pdf::GetPDFDocInfo(pdf_region.mapping.GetMemoryAsSpan<uint8_t>(),
                                &num_pages, &max_width_in_points));
  EXPECT_EQ(3, num_pages);
  EXPECT_DOUBLE_EQ(612.0, max_width_in_points);

  base::ReadOnlySharedMemoryRegion nup_pdf_region;
  base::Optional<mojom::PdfNupConverter::Status> status = Convert(
      std::move(pdf_region.region), /*pages_per_sheet=*/2, &nup_pdf_region);

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(mojom::PdfNupConverter::Status::SUCCESS, status.value());
  base::ReadOnlySharedMemoryMapping nup_pdf_mapping = nup_pdf_region.Map();
  ASSERT_TRUE(nup_pdf_mapping.IsValid());

  // For 2-up, a 3 page portrait document fits on 2 landscape-oriented pages.
  ASSERT_TRUE(
      chrome_pdf::GetPDFDocInfo(nup_pdf_mapping.GetMemoryAsSpan<uint8_t>(),
                                &num_pages, &max_width_in_points));
  EXPECT_EQ(2, num_pages);
  EXPECT_DOUBLE_EQ(3300.0, max_width_in_points);
}

IN_PROC_BROWSER_TEST_F(PdfNupConverterClientBrowserTest,
                       DocumentConvert4UpSuccess) {
  base::MappedReadOnlyRegion pdf_region =
      GetPdfRegion("pdf_converter_basic.pdf");
  ASSERT_TRUE(pdf_region.IsValid());

  // Make sure pdf_converter_basic.pdf has 3 pages.
  int num_pages;
  double max_width_in_points;
  ASSERT_TRUE(
      chrome_pdf::GetPDFDocInfo(pdf_region.mapping.GetMemoryAsSpan<uint8_t>(),
                                &num_pages, &max_width_in_points));
  EXPECT_EQ(3, num_pages);
  EXPECT_DOUBLE_EQ(612.0, max_width_in_points);

  base::ReadOnlySharedMemoryRegion nup_pdf_region;
  base::Optional<mojom::PdfNupConverter::Status> status = Convert(
      std::move(pdf_region.region), /*pages_per_sheet=*/4, &nup_pdf_region);

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(mojom::PdfNupConverter::Status::SUCCESS, status.value());
  base::ReadOnlySharedMemoryMapping nup_pdf_mapping = nup_pdf_region.Map();
  ASSERT_TRUE(nup_pdf_mapping.IsValid());

  // For 4-up, a 3 page portrait document fits on 1 landscape-oriented page.
  ASSERT_TRUE(
      chrome_pdf::GetPDFDocInfo(nup_pdf_mapping.GetMemoryAsSpan<uint8_t>(),
                                &num_pages, &max_width_in_points));
  EXPECT_EQ(1, num_pages);
  EXPECT_DOUBLE_EQ(2550.0, max_width_in_points);
}

IN_PROC_BROWSER_TEST_F(PdfNupConverterClientBrowserTest,
                       DocumentConvertBadData) {
  base::MappedReadOnlyRegion pdf_region = GetBadDataRegion();
  ASSERT_TRUE(pdf_region.IsValid());

  base::ReadOnlySharedMemoryRegion nup_pdf_region;
  base::Optional<mojom::PdfNupConverter::Status> status = Convert(
      std::move(pdf_region.region), /*pages_per_sheet=*/2, &nup_pdf_region);

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(mojom::PdfNupConverter::Status::CONVERSION_FAILURE, status.value());
  base::ReadOnlySharedMemoryMapping nup_pdf_mapping = nup_pdf_region.Map();
  EXPECT_FALSE(nup_pdf_mapping.IsValid());
}

}  // namespace printing
