// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_PLUGIN_BASE_H_
#define PDF_PDF_VIEW_PLUGIN_BASE_H_

#include "pdf/pdf_engine.h"

#include <memory>

namespace chrome_pdf {

class PDFiumEngine;

// Common base to share code between the two plugin implementations,
// `OutOfProcessInstance` (Pepper) and `PdfViewWebPlugin` (Blink).
class PdfViewPluginBase : public PDFEngine::Client {
 public:
  PdfViewPluginBase(const PdfViewPluginBase& other) = delete;
  PdfViewPluginBase& operator=(const PdfViewPluginBase& other) = delete;

 protected:
  PdfViewPluginBase();
  ~PdfViewPluginBase() override;

  // Initializes the main `PDFiumEngine`. If `enable_javascript` is true, the
  // engine will support executing JavaScript.
  //
  // Any existing engine will be replaced.
  void InitializeEngine(bool enable_javascript);

  // Destroys the main `PDFiumEngine`. Subclasses should call this method in
  // their destructor to ensure the engine is destroyed first.
  void DestroyEngine();

  PDFiumEngine* engine() { return engine_.get(); }

 private:
  std::unique_ptr<PDFiumEngine> engine_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_PLUGIN_BASE_H_
