// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_FEATURES_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_FEATURES_H_

#include <string>
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// # Encoding of IdentifiableSurfaces:
//
// An IdentifiableSurface is encoded as the decimal representation of it's
// ToUkmMetricHash() result.

// Parent feature for all identifiability study logic.
//
// If the feature is disabled, then this browser instance will not be
// participating in the identifiability study. I.e. no identifiability metrics
// will be recorded or reported.
//
// Enabling the feature doesn't automatically make this client part of the study
// either. See documentation for IdentifiabilityStudySettings for a full list of
// the criteria that contributes to that decision.
extern const base::Feature kIdentifiabilityStudy;

// Each time the key study parameters change, the study generation also
// increments. Reporting the study generation alongside metrics allows the data
// from different study generations to be grouped independently.
//
// Changes to the study generation resets all persisted state for the
// identifiability study.
//
// Parameter name: "Gen"
// Parameter type: int
extern const base::FeatureParam<int> kIdentifiabilityStudyGeneration;

// Surfaces that should be excluded from reports.
//
// Parameter name: "BlockedHashes"
// Parameter type: Comma separated list of decimal integers, each of which
//     represents an IdentifiableSurface.
//
// E.g.:
//  * "258, 257" : Matches IdentifiableSurface::FromTypeAndInput(kWebFeature, 1)
//                 and IdentifiableSurface::FromTypeAndInput(kWebFeature, 2)
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudyBlockedMetrics;

// Surfaces that should be excluded from reports.
//
// Parameter name: "BlockedTypes"
// Parameter type: Comma separated list of decimal integers, each of which
//     represents an IdentifiableSurface::Type.
//
// E.g.:
//  * "1, 2" : Matches all surfaces with types kWebFeature and kCanvasReadback.
extern const base::FeatureParam<std::string> kIdentifiabilityStudyBlockedTypes;

// Base selection rate for surfaces that don't have a per-surface selection
// rate. Rates are always represented as the denominator of a 1-in-N selection.
//
// Parameter name: "Rho"
// Parameter type: int
extern const base::FeatureParam<int> kIdentifiabilityStudySurfaceSelectionRate;

// The maximum number of surfaces that can be included in the identifiability
// study.
//
// Parameter name: "Max"
// Parameter type: int
extern const base::FeatureParam<int> kIdentifiabilityStudyMaxSurfaces;

// Selection rate for clusters of related surfaces.
//
// Parameter name: "HashRate"
// Parameter type: Comma separated list of <filter-string>;<rate> pairs.
//
// E.g.:
//   * "257;800" : Sets the selection rate to 1-in-800 for
//        IdentifiableSurface::FromTypeAndInput(kWebFeature, 1).
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudyPerSurfaceSettings;

// Selection rate for clusters of related surfaces.
//
// Parameter name: "TypeRate"
// Parameter type: Comma separated list of <filter-string>;<rate> pairs.
//
// E.g.:
//   * "2:1000" : Sets the selection rate to 1-in-1000 for *all* surfaces with
//        type kCanvasReadback.
extern const base::FeatureParam<std::string>
    kIdentifiabilityStudyPerTypeSettings;

}  // namespace features

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_FEATURES_H_
