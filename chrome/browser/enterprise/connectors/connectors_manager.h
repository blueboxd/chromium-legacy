// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include <set>

#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "chrome/browser/enterprise/connectors/analysis_service_settings.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/gurl.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

// Controls whether the Enterprise Connectors policies should be read by
// ConnectorsManager. Legacy policies will be read as a fallback if this feature
// is disabled.
extern const base::Feature kEnterpriseConnectorsEnabled;

// Manages access to Connector policies. This class is responsible for caching
// the Connector policies, validate them against approved service providers and
// provide a simple interface to them.
class ConnectorsManager {
 public:
  // Map used to cache analysis connectors settings.
  using AnalysisConnectorsSettings =
      std::map<AnalysisConnector, std::vector<AnalysisServiceSettings>>;

  static ConnectorsManager* GetInstance();

  // Validates which settings should be applied to an analysis connector event
  // against cached policies. This function will prioritize new connector
  // policies over legacy ones if they are set.
  base::Optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      AnalysisConnector connector);

  // Checks if the corresponding connector is enabled.
  bool IsConnectorEnabled(AnalysisConnector connector);

  bool DelayUntilVerdict(AnalysisConnector connector) const;

  // Public legacy functions.
  // These functions are used to interact with legacy policies and should only
  // be called while the connectors equivalent isn't available. They should be
  // removed once legacy policies are deprecated.

  // Check a url against the corresponding URL patterns policies.
  bool MatchURLAgainstLegacyDlpPolicies(const GURL& url, bool upload) const;
  bool MatchURLAgainstLegacyMalwarePolicies(const GURL& url, bool upload) const;

  // Public testing functions.
  const AnalysisConnectorsSettings& GetAnalysisConnectorsSettingsForTesting()
      const;

  // Helpers to reset the ConnectorManager instance across test since it's a
  // singleton that would otherwise persist its state.
  void SetUpForTesting();
  void TearDownForTesting();

 private:
  friend struct base::DefaultSingletonTraits<ConnectorsManager>;

  // Constructor and destructor are declared as private so callers use
  // GetInstance instead.
  ConnectorsManager();
  ~ConnectorsManager();

  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  base::Optional<AnalysisSettings> GetAnalysisSettingsFromConnectorPolicy(
      const GURL& url,
      AnalysisConnector connector);

  // Read and cache the policy corresponding to |connector|.
  void CacheConnectorPolicy(AnalysisConnector connector);

  // Sets up |pref_change_registrar_| if kEnterpriseConntorsEnabled is true.
  // Used by the constructor and SetUpForTesting.
  void StartObservingPrefs();
  void StartObservingPref(AnalysisConnector connector);

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

  // Cached values of the connector policies. Updated when a connector is first
  // used or when a policy is updated.
  AnalysisConnectorsSettings connector_settings_;

  // Used to track changes of connector policies and propagate them in
  // |connector_settings_|.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
