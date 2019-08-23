// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_protocol_encoding.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"

namespace ui_devtools {
namespace {
using ::inspector_protocol_encoding::span;
using IPEStatus = ::inspector_protocol_encoding::Status;

// Platform allows us to inject the string<->double conversion
// routines from base:: into the inspector_protocol JSON parser / serializer.
class Platform : public ::inspector_protocol_encoding::json::Platform {
 public:
  bool StrToD(const char* str, double* result) const override {
    return base::StringToDouble(str, result);
  }

  // Prints |value| in a format suitable for JSON.
  std::unique_ptr<char[]> DToStr(double value) const override {
    std::string str = base::NumberToString(value);
    std::unique_ptr<char[]> result(new char[str.size() + 1]);
    memcpy(result.get(), str.c_str(), str.size() + 1);
    return result;
  }
};
}  // namespace

IPEStatus ConvertCBORToJSON(span<uint8_t> cbor, std::string* json) {
  Platform platform;
  return ::inspector_protocol_encoding::json::ConvertCBORToJSON(platform, cbor,
                                                                json);
}

IPEStatus ConvertJSONToCBOR(span<uint8_t> json, std::string* cbor) {
  Platform platform;
  return ConvertJSONToCBOR(platform, json, cbor);
}

IPEStatus ConvertJSONToCBOR(span<uint8_t> json, std::vector<uint8_t>* cbor) {
  Platform platform;
  return ConvertJSONToCBOR(platform, json, cbor);
}

}  // namespace ui_devtools
