// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/uri_components.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  chromeos::Printer printer("fuzz");
  printer.set_uri(std::string(reinterpret_cast<const char*>(data), size));
  printer.GetHostAndPort();
  printer.GetProtocol();
  base::Optional<chromeos::UriComponents> components =
      printer.GetUriComponents();
  if (components) {
    bool encrypted = components->encrypted();
    std::string scheme = components->scheme();
    std::string host = components->host();
    int port = components->port();
    std::string path = components->path();
    std::string reconstructed =
        base::StrCat({encrypted ? "True" : "False", scheme, host,
                      base::NumberToString(port), path});
  }
  return 0;
}
