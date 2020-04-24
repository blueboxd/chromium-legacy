// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_cros_config.h"

namespace arc {

FakeCrosConfig::FakeCrosConfig() = default;

FakeCrosConfig::~FakeCrosConfig() = default;

bool FakeCrosConfig::GetString(const std::string& path,
                               const std::string& property,
                               std::string* val_out) {
  auto it = overrides_.find(property);
  if (it != overrides_.end()) {
    *val_out = it->second;
    return true;
  }
  return arc::CrosConfig::GetString(path, property, val_out);
}

void FakeCrosConfig::SetString(const std::string& path,
                               const std::string& property,
                               const std::string& value) {
  overrides_.emplace(property, value);
}

}  // namespace arc
