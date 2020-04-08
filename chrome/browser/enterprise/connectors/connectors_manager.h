// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

// Enums representing each connector to be used as arguments so the manager can
// read the appropriate policies/settings.
enum class AnalysisConnector {
  FILE_DOWNLOADED,
  FILE_ATTACHED,
  BULK_DATA_ENTRY,
};

enum class ReportingConnector {
  SECURITY_EVENT,
};

// Enum representing if an analysis should block further interactions with the
// browser until its verdict is obtained.
enum class BlockUntilVerdict {
  NO_BLOCK = 0,
  BLOCK = 1,
};

// Manages access to Connector policies. This class is responsible for caching
// the Connector policies, validate them against approved service providers and
// provide a simple interface to them.
class ConnectorsManager {
 public:
  // Structs representing settings to be used for an analysis or a report. These
  // settings should only be kept and considered valid for the specific
  // analysis/report they were obtained for.
  struct AnalysisSettings {
    AnalysisSettings();
    AnalysisSettings(AnalysisSettings&&);
    AnalysisSettings& operator=(AnalysisSettings&&);
    ~AnalysisSettings();

    GURL analysis_url;
    std::set<std::string> tags;
    BlockUntilVerdict block_until_verdict;
    bool block_password_protected_files;
    bool block_large_files;
    bool block_unsupported_file_types;
  };

  struct ReportingSettings {
    ReportingSettings();
    ReportingSettings(ReportingSettings&&);
    ReportingSettings& operator=(ReportingSettings&&);
    ~ReportingSettings();

    std::vector<GURL> reporting_urls;
  };

  // Callback used to retrieve AnalysisSettings objects from the manager
  // asynchronously. base::nullopt means no analysis should take place.
  using AnalysisSettingsCallback =
      base::OnceCallback<void(base::Optional<AnalysisSettings>)>;

  static ConnectorsManager* GetInstance();

  // Validates which settings should be applied to an analysis connector event
  // against cached policies.
  void GetAnalysisSettings(const GURL& url,
                           AnalysisConnector connector,
                           AnalysisSettingsCallback callback);

  // Public legacy functions.
  // These functions are used to interact with legacy policies and should only
  // be called while the connectors equivalent isn't available. They should be
  // removed once legacy policies are deprecated.

  // Check a url against the corresponding URL patterns policies.
  bool MatchURLAgainstLegacyDlpPolicies(const GURL& url, bool upload) const;
  bool MatchURLAgainstLegacyMalwarePolicies(const GURL& url, bool upload) const;

 private:
  friend struct base::DefaultSingletonTraits<ConnectorsManager>;

  // Constructor and destructor are declared as private so callers use
  // GetInstance instead.
  ConnectorsManager();
  ~ConnectorsManager();

  // Private legacy functions.
  // These functions are used to interact with legacy policies and should stay
  // private. They should be removed once legacy policies are deprecated.

  // Returns analysis settings based on legacy policies.
  base::Optional<AnalysisSettings> GetAnalysisSettingsFromLegacyPolicies(
      const GURL& url,
      AnalysisConnector connector) const;

  BlockUntilVerdict LegacyBlockUntilVerdict(bool upload) const;
  bool LegacyBlockPasswordProtectedFiles(bool upload) const;
  bool LegacyBlockLargeFiles(bool upload) const;
  bool LegacyBlockUnsupportedFileTypes(bool upload) const;

  std::set<std::string> MatchURLAgainstLegacyPolicies(const GURL& url,
                                                      bool upload) const;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
