// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTER_CLIENT_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTER_CLIENT_H_

#include "base/macros.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace base {
class FilePath;
}

namespace credential_provider {

class GcpCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  GcpCrashReporterClient() = default;
  ~GcpCrashReporterClient() override;

  // crash_reporter::CrashReporterClient:
  bool ShouldCreatePipeName(const base::string16& process_type) override;
  bool GetAlternativeCrashDumpLocation(base::string16* crash_dir) override;
  void GetProductNameAndVersion(const base::string16& exe_path,
                                base::string16* product_name,
                                base::string16* version,
                                base::string16* special_build,
                                base::string16* channel_name) override;
  bool ShouldShowRestartDialog(base::string16* title,
                               base::string16* message,
                               bool* is_rtl_locale) override;
  bool AboutToRestart() override;
  bool GetIsPerUserInstall() override;
  bool GetShouldDumpLargerDumps() override;
  int GetResultCodeRespawnFailed() override;
  bool GetCrashDumpLocation(base::string16* crash_dir) override;
  bool IsRunningUnattended() override;
  bool GetCollectStatsConsent() override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;

 protected:
  virtual base::FilePath GetPathForFileVersionInfo(
      const base::string16& exe_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(GcpCrashReporterClient);
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTER_CLIENT_H_
