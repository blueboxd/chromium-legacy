// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/cups_util.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/cups_jobs.h"

namespace cups_proxy {

base::Optional<IppResponse> BuildGetDestsResponse(
    const IppRequest& request,
    const std::vector<chromeos::Printer>& printers) {
  IppResponse ret;

  // Standard OK status line.
  ret.status_line = HttpStatusLine{"HTTP/1.1", "200", "OK"};

  // Generic HTTP headers.
  ret.headers = std::vector<ipp_converter::HttpHeader>{
      {"Content-Language", "en"},
      {"Content-Type", "application/ipp"},
      {"Server", "CUPS/2.1 IPP/2.1"},
      {"X-Frame-Options", "DENY"},
      {"Content-Security-Policy", "frame-ancestors 'none'"}};

  // Fill in IPP attributes.
  ret.ipp = printing::WrapIpp(ippNewResponse(request.ipp.get()));
  for (const auto& printer : printers) {
    std::string printer_uri = printing::PrinterUriFromName(printer.id());

    // Setting the printer-uri.
    ippAddString(ret.ipp.get(), IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "printer-uri-supported", nullptr, printer_uri.c_str());

    // Setting the display name.
    ippAddString(ret.ipp.get(), IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name",
                 nullptr, printer.display_name().c_str());

    // Optional setting of the make_and_model, if known.
    if (!printer.make_and_model().empty()) {
      ippAddString(ret.ipp.get(), IPP_TAG_PRINTER, IPP_TAG_TEXT,
                   "printer-make-and-model", nullptr,
                   printer.make_and_model().c_str());
    }
  }

  // Add the final content length into headers
  const size_t ipp_metadata_sz = ippLength(ret.ipp.get());
  ret.headers.push_back(
      {"Content-Length", base::NumberToString(ipp_metadata_sz)});

  // Build parsed response buffer
  // Note: We are reusing the HTTP request building function since the responses
  // have the exact same format.
  auto response_buffer = ipp_converter::BuildIppRequest(
      ret.status_line.http_version, ret.status_line.status_code,
      ret.status_line.reason_phrase, ret.headers, ret.ipp.get(), ret.ipp_data);
  if (!response_buffer) {
    return base::nullopt;
  }

  ret.buffer = std::move(*response_buffer);
  return ret;
}

base::Optional<std::string> GetPrinterId(ipp_t* ipp) {
  // We expect the printer id to be embedded in the printer-uri.
  ipp_attribute_t* printer_uri_attr =
      ippFindAttribute(ipp, "printer-uri", IPP_TAG_URI);
  if (!printer_uri_attr) {
    return base::nullopt;
  }

  // Only care about the resource, throw everything else away
  char resource[HTTP_MAX_URI], unwanted_buffer[HTTP_MAX_URI];
  int unwanted_port;

  std::string printer_uri = ippGetString(printer_uri_attr, 0, NULL);
  httpSeparateURI(HTTP_URI_CODING_RESOURCE, printer_uri.data(), unwanted_buffer,
                  HTTP_MAX_URI, unwanted_buffer, HTTP_MAX_URI, unwanted_buffer,
                  HTTP_MAX_URI, &unwanted_port, resource, HTTP_MAX_URI);

  // The printer id should be the last component of the resource.
  base::StringPiece uuid(resource);
  auto uuid_start = uuid.find_last_of('/');
  if (uuid_start == base::StringPiece::npos) {
    return base::nullopt;
  }

  return uuid.substr(uuid_start + 1).as_string();
}

base::Optional<std::string> ParseEndpointForPrinterId(
    base::StringPiece endpoint) {
  size_t last_path = endpoint.find_last_of('/');
  if (last_path == base::StringPiece::npos ||
      last_path + 1 >= endpoint.size()) {
    return base::nullopt;
  }

  endpoint.remove_prefix(last_path + 1);
  return endpoint.as_string();
}

}  // namespace cups_proxy
