// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/common/chrome_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

bool IsolatedPrerenderIsEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatePrerenders);
}

base::Optional<GURL> IsolatedPrerenderProxyServer() {
  if (!base::FeatureList::IsEnabled(features::kIsolatedPrerenderUsesProxy))
    return base::nullopt;

  GURL url(base::GetFieldTrialParamValueByFeature(
      features::kIsolatedPrerenderUsesProxy, "proxy_server_url"));
  if (!url.is_valid() || !url.has_host() || !url.has_scheme())
    return base::nullopt;
  return url;
}

bool IsolatedPrerenderShouldReplaceDataReductionCustomProxy() {
  bool replace =
      data_reduction_proxy::params::IsIncludedInHoldbackFieldTrial() &&
      IsolatedPrerenderIsEnabled() &&
      IsolatedPrerenderProxyServer().has_value();
  // TODO(robertogden): Remove this once all pieces are landed.
  DCHECK(!replace);
  return replace;
}
