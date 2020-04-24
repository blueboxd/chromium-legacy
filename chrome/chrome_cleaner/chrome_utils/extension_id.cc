// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/extension_id.h"

#include <string>

#include "base/optional.h"

namespace chrome_cleaner {

base::Optional<ExtensionID> ExtensionID::Create(const std::string& value) {
  if (ExtensionID::IsValidID(value)) {
    return {ExtensionID(value)};
  }
  return {};
}

ExtensionID::ExtensionID(const std::string& value) : value_(value) {}

bool ExtensionID::IsValidID(const std::string& value) {
  if (value.length() < 32) {
    return false;
  }
  for (const auto& character : value) {
    if (character < 'a' || character > 'p') {
      return false;
    }
  }
  return true;
}

std::string ExtensionID::AsString() const {
  return value_;
}

}  // namespace chrome_cleaner
