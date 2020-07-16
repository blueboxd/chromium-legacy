// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
#define EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/document_scan.h"

namespace extensions {

namespace api {

class DocumentScanScanFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("documentScan.scan", DOCUMENT_SCAN_SCAN)
  DocumentScanScanFunction();
  DocumentScanScanFunction(const DocumentScanScanFunction&) = delete;
  DocumentScanScanFunction& operator=(const DocumentScanScanFunction&) = delete;

 protected:
  ~DocumentScanScanFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  friend class DocumentScanScanFunctionTest;

  void OnScannerListReceived(
      base::Optional<lorgnette::ListScannersResponse> response);
  void OnResultsReceived(base::Optional<std::string> scanned_image);

  std::unique_ptr<document_scan::Scan::Params> params_;
};

}  // namespace api

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
