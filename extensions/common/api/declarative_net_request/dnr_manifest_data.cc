// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"

#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {
namespace declarative_net_request {

DNRManifestData::DNRManifestData(std::vector<RulesetInfo> rulesets)
    : rulesets(std::move(rulesets)) {}
DNRManifestData::~DNRManifestData() = default;

// static
const std::vector<DNRManifestData::RulesetInfo>& DNRManifestData::GetRulesets(
    const Extension& extension) {
  // Since we return a reference, use a function local static for the case where
  // the extension didn't specify any rulesets.
  static const base::NoDestructor<std::vector<DNRManifestData::RulesetInfo>>
      empty_vector;

  Extension::ManifestData* data =
      extension.GetManifestData(manifest_keys::kDeclarativeNetRequestKey);
  if (!data)
    return *empty_vector;

  return static_cast<DNRManifestData*>(data)->rulesets;
}

}  // namespace declarative_net_request
}  // namespace extensions
