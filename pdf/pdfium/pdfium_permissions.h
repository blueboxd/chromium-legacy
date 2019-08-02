// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_PERMISSIONS_H_
#define PDF_PDFIUM_PDFIUM_PERMISSIONS_H_

#include "pdf/pdf_engine.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

// The permissions for a given FPDF_DOCUMENT.
class PDFiumPermissions final {
 public:
  explicit PDFiumPermissions(FPDF_DOCUMENT doc);

  bool HasPermission(PDFEngine::DocumentPermission permission) const;

 private:
  // Permissions security handler revision number. -1 for unknown.
  const int permissions_handler_revision_;

  // Permissions bitfield.
  const unsigned long permission_bits_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_PERMISSIONS_H_
