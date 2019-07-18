// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

class ReportGenerator {
 public:
  ReportGenerator();
  ~ReportGenerator();

  std::vector<std::unique_ptr<em::ChromeDesktopReportRequest>> Generate();

 protected:
  // Creates a basic request that will be used by all Profiles.
  void CreateBasicRequest();

  // Returns an OS report contains basic OS information includes OS name, OS
  // architecture and OS version.
  virtual std::unique_ptr<em::OSReport> GetOSReport();

  // Returns the name of computer.
  virtual std::string GetMachineName();

  // Returns the name of OS user.
  virtual std::string GetOSUserName();

  // Returns the Serial number of the device. It's Windows only field and empty
  // on other platforms.
  virtual std::string GetSerialNumber();

  // Returns a browser report contains browser related information includes
  // browser version, channel and executable path.
  virtual std::unique_ptr<em::BrowserReport> GetBrowserReport();

  // Returns the list of Profiles that is owned by current Browser instance. It
  // only contains Profile's path and name.
  std::vector<std::unique_ptr<em::ChromeUserProfileInfo>> GetProfiles();

 private:
  std::vector<std::unique_ptr<em::ChromeDesktopReportRequest>> requests_;

  // Basic information that is shared among requests.
  em::ChromeDesktopReportRequest basic_request_;

  DISALLOW_COPY_AND_ASSIGN(ReportGenerator);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_
