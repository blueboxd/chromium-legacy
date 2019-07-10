// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/cups_util.h"

#include "base/strings/string_piece.h"

namespace cups_proxy {

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
