// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <memory>

#include "pdf/pdfium/pdfium_engine.h"

namespace chrome_pdf {

PdfViewPluginBase::PdfViewPluginBase() = default;

PdfViewPluginBase::~PdfViewPluginBase() = default;

void PdfViewPluginBase::InitializeEngine(bool enable_javascript) {
  engine_ = std::make_unique<PDFiumEngine>(this, enable_javascript);
}

void PdfViewPluginBase::DestroyEngine() {
  engine_.reset();
}

}  // namespace chrome_pdf
