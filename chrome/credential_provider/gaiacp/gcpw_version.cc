// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcpw_version.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"

namespace credential_provider {

GcpwVersion::GcpwVersion() : version_{0, 0, 0, 0} {}

GcpwVersion::GcpwVersion(const std::string& version_str) : GcpwVersion() {
  if (version_str.empty())
    return;

  std::vector<std::string> components = base::SplitString(
      version_str, ".", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < std::min(version_.size(), components.size()); ++i) {
    unsigned value;
    if (base::StringToUint(components[i], &value))
      version_[i] = value;
  }
}

GcpwVersion::GcpwVersion(const GcpwVersion& other) : version_(other.version_) {}

std::string GcpwVersion::ToString() const {
  return base::StringPrintf("%d.%d.%d.%d", version_[0], version_[1],
                            version_[2], version_[3]);
}

unsigned GcpwVersion::major() const {
  return version_[0];
}

unsigned GcpwVersion::minor() const {
  return version_[1];
}

unsigned GcpwVersion::build() const {
  return version_[2];
}

unsigned GcpwVersion::patch() const {
  return version_[3];
}

bool GcpwVersion::operator==(const GcpwVersion& other) const {
  return version_ == other.version_;
}

GcpwVersion& GcpwVersion::operator=(const GcpwVersion& other) {
  version_ = other.version_;
  return *this;
}

bool GcpwVersion::operator<(const GcpwVersion& other) const {
  for (size_t i = 0; i < version_.size(); ++i) {
    if (version_[i] < other.version_[i]) {
      return true;
    } else if (version_[i] > other.version_[i]) {
      return false;
    }
  }
  return false;
}

}  // namespace credential_provider
