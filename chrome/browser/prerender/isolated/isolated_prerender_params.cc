// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"

base::Optional<GURL> IsolatedPrerenderProxyServer() {
  if (!base::FeatureList::IsEnabled(features::kIsolatedPrerenderUsesProxy))
    return base::nullopt;

  GURL url(base::GetFieldTrialParamValueByFeature(
      features::kIsolatedPrerenderUsesProxy, "proxy_server_url"));
  if (!url.is_valid() || !url.has_host() || !url.has_scheme())
    return base::nullopt;
  return url;
}
