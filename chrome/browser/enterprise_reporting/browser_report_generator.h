// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "ppapi/buildflags/buildflags.h"

namespace content {
struct WebPluginInfo;
}

namespace enterprise_reporting {

// A report generator that collects Browser related information.
class BrowserReportGenerator {
 public:
  using ReportCallback = base::OnceCallback<void(
      std::unique_ptr<enterprise_management::BrowserReport>)>;

  BrowserReportGenerator();
  ~BrowserReportGenerator();

  // Generates a BrowserReport with the following fields:
  // - browser_version, channel, executable_path
  // - user profiles: id, name, is_full_report (always be false).
  // - plugins: name, version, filename, description.
  void Generate(ReportCallback callback);

 private:
  // Generates plugin info in the given report instance, if needed. Passes
  // |report| to |callback| either asynchronously when the plugin info is
  // available, or synchronously otherwise.
  void GeneratePluginsIfNeeded(
      ReportCallback callback,
      std::unique_ptr<enterprise_management::BrowserReport> report);

#if BUILDFLAG(ENABLE_PLUGINS)
  // Populates |report| with the plugin info in |plugins|, then passes the
  // report to |callback|.
  void OnPluginsReady(
      ReportCallback callback,
      std::unique_ptr<enterprise_management::BrowserReport> report,
      const std::vector<content::WebPluginInfo>& plugins);
#endif

  base::WeakPtrFactory<BrowserReportGenerator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserReportGenerator);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_REPORT_GENERATOR_H_
