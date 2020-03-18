// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

class Profile;

namespace enterprise_management {
class AppInfo;
}  // namespace enterprise_management

namespace policy {

// A class that is responsible for collecting application inventory and usage
// information.
class AppInfoGenerator {
 public:
  explicit AppInfoGenerator(Profile* profile);
  AppInfoGenerator(const AppInfoGenerator&) = delete;
  AppInfoGenerator& operator=(const AppInfoGenerator&) = delete;
  ~AppInfoGenerator() = default;

  std::vector<enterprise_management::AppInfo> Generate() const;

 private:
  Profile* const profile_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_
