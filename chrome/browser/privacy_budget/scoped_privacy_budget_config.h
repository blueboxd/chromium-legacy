// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_SCOPED_PRIVACY_BUDGET_CONFIG_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_SCOPED_PRIVACY_BUDGET_CONFIG_H_

#include <limits>
#include <map>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace test {

// Configures the privacy budget feature settings. One can also configure the
// service manually by controlling the global feature lists directly. This class
// is merely a convenience.
//
// For testing only.
//
// Note 1: Since we are changing feature lists, the same caveats as
//     base::test::ScopedFeatureList apply. E.g. The Apply() method should be
//     called prior to starting the threads in multi threaded environments.
//
// Note 2: The configuration applied by this class is *scoped*. The destructor
//     reverts the configuration changes.
class ScopedPrivacyBudgetConfig {
 public:
  // These fields correspond to the equivalent features described in
  // privacy_budget_features.h
  //
  // The default values enable the identifiability study with a sampling rate of
  // 1, which means every surface is included in UKM reports.
  struct Parameters {
    Parameters();
    Parameters(const Parameters&);
    Parameters(Parameters&&);
    ~Parameters();

    bool enabled = true;
    int generation = 1;

    std::vector<blink::IdentifiableSurface> blocked_surfaces;
    std::vector<blink::IdentifiableSurface::Type> blocked_types;
    int surface_selection_rate = 1;
    int max_surfaces = std::numeric_limits<int>::max();
    std::map<blink::IdentifiableSurface, int> per_surface_sampling_rate;
    std::map<blink::IdentifiableSurface::Type, int> per_type_sampling_rate;
  };

  // Doesn't do anything until Apply() is called.
  ScopedPrivacyBudgetConfig();

  // Constructs and applies the configuration described in |parameters|. No need
  // to call Apply()
  explicit ScopedPrivacyBudgetConfig(const Parameters& parameters);

  ~ScopedPrivacyBudgetConfig();

  // Apply the configuration as described in |parameters|. Should only be called
  // once.
  void Apply(const Parameters& parameters);

  ScopedPrivacyBudgetConfig(const ScopedPrivacyBudgetConfig&) = delete;
  ScopedPrivacyBudgetConfig& operator=(const ScopedPrivacyBudgetConfig&) =
      delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace test

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_SCOPED_PRIVACY_BUDGET_CONFIG_H_
