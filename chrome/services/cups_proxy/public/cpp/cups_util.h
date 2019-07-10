// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_CUPS_UTIL_H_
#define CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_CUPS_UTIL_H_

#include <cups/cups.h>
#include <stddef.h>

#include <string>

#include "base/optional.h"

// Utility namespace that encapsulates helpful libCUPS-dependent
// constants/methods.
namespace cups_proxy {

// Max HTTP buffer size, as defined libCUPS at cups/http.h.
// Note: This is assumed to be stable.
static const size_t kHttpMaxBufferSize = 2048;

// If |ipp| refers to a printer, we return the associated printer_id.
// Note: Expects the printer id to be embedded in the resource field of the
// 'printer-uri' IPP attribute.
// TODO(crbug.com/945409): Expand testing suite.
base::Optional<std::string> GetPrinterId(ipp_t* ipp);

// Expects |endpoint| to be of the form '/printers/{printer_id}'.
// Returns an empty Optional if parsing fails or yields an empty printer_id.
base::Optional<std::string> ParseEndpointForPrinterId(
    base::StringPiece endpoint);

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_PUBLIC_CPP_CUPS_UTIL_H_
