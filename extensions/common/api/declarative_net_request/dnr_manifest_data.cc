// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"

#include <utility>

#include "base/logging.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {
namespace declarative_net_request {

DNRManifestData::DNRManifestData(std::vector<RulesetInfo> rulesets)
    : rulesets(std::move(rulesets)) {
  // TODO(crbug.com/953894): Remove this DCHECK when we support specifying 0
  // rulesets.
  DCHECK(!(this->rulesets.empty()));
}
DNRManifestData::~DNRManifestData() = default;

// static
bool DNRManifestData::HasRuleset(const Extension& extension) {
  return extension.GetManifestData(manifest_keys::kDeclarativeNetRequestKey);
}

// static
const std::vector<DNRManifestData::RulesetInfo>& DNRManifestData::GetRulesets(
    const Extension& extension) {
  Extension::ManifestData* data =
      extension.GetManifestData(manifest_keys::kDeclarativeNetRequestKey);
  DCHECK(data);

  return static_cast<DNRManifestData*>(data)->rulesets;
}

}  // namespace declarative_net_request
}  // namespace extensions
