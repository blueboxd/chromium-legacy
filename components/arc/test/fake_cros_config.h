// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_CROS_CONFIG_H_
#define COMPONENTS_ARC_TEST_FAKE_CROS_CONFIG_H_

#include <map>
#include <string>

#include "components/arc/session/arc_property_util.h"

namespace arc {

// A fake arc::CrosConfig used for testing.
class FakeCrosConfig : public arc::CrosConfig {
 public:
  FakeCrosConfig();
  ~FakeCrosConfig() override;
  FakeCrosConfig(const FakeCrosConfig&) = delete;
  FakeCrosConfig& operator=(const FakeCrosConfig&) = delete;

  // arc::CrosConfig:
  bool GetString(const std::string& path,
                 const std::string& property,
                 std::string* val_out) override;

  // Sets the value of a property specified by |path| and |property| to |value|.
  void SetString(const std::string& path,
                 const std::string& property,
                 const std::string& value);

 private:
  // A map of overridden properties to their values.
  std::map<std::string, std::string> overrides_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_CROS_CONFIG_H_
