// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_

#include <memory>
#include <queue>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/browser_report_generator.h"
#include "chrome/browser/enterprise/reporting/report_request_definition.h"
#include "chrome/browser/enterprise/reporting/report_request_queue_generator.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_reporting {

class ReportGenerator {
 public:
  using ReportRequest = definition::ReportRequest;
  using ReportRequests = std::queue<std::unique_ptr<ReportRequest>>;
  using ReportCallback = base::OnceCallback<void(ReportRequests)>;

  ReportGenerator();
  virtual ~ReportGenerator();

  // Asynchronously generates a queue of report requests, providing them to
  // |callback| when ready. If |with_profiles| is true, full details are
  // included for all loaded profiles; otherwise, only profile name and path
  // are included.
  virtual void Generate(bool with_profiles, ReportCallback callback);

  void SetMaximumReportSizeForTesting(size_t size);

 protected:
  // Creates a basic request that will be used by all Profiles.
  void CreateBasicRequest(std::unique_ptr<ReportRequest> basic_request,
                          bool with_profiles,
                          ReportCallback callback);

  // Returns an OS report contains basic OS information includes OS name, OS
  // architecture and OS version.
  virtual std::unique_ptr<enterprise_management::OSReport> GetOSReport();

  // Returns the name of computer.
  virtual std::string GetMachineName();

  // Returns the name of OS user.
  virtual std::string GetOSUserName();

  // Returns the Serial number of the device. It's Windows only field and empty
  // on other platforms.
  virtual std::string GetSerialNumber();

#if defined(OS_CHROMEOS)
  // Collect the Android application information installed on primary profile,
  // and set it to |basic_request_|.
  virtual void SetAndroidAppInfos(ReportRequest* basic_request);
#endif

 private:
  void OnBrowserReportReady(
      bool with_profiles,
      ReportCallback callback,
      std::unique_ptr<ReportRequest> basic_request,
      std::unique_ptr<enterprise_management::BrowserReport> browser_report);

  ReportRequestQueueGenerator report_request_queue_generator_;
  BrowserReportGenerator browser_report_generator_;

  base::WeakPtrFactory<ReportGenerator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReportGenerator);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_
