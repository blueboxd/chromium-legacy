// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "url/gurl.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

// Manages access to Connector policies. This class is responsible for caching
// the Connector policies, validate them against approved service providers and
// provide a simple interface to them.
class ConnectorsManager {
 public:
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
