// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"

#include <utility>
#include "extensions/common/manifest_constants.h"

namespace extensions {
namespace declarative_net_request {

DNRManifestData::DNRManifestData(RulesetInfo ruleset)
    : ruleset(std::move(ruleset)) {}
DNRManifestData::~DNRManifestData() = default;

// static
bool DNRManifestData::HasRuleset(const Extension& extension) {
  return extension.GetManifestData(manifest_keys::kDeclarativeNetRequestKey);
}

// static
base::FilePath DNRManifestData::GetRulesetPath(const Extension& extension) {
  Extension::ManifestData* data =
      extension.GetManifestData(manifest_keys::kDeclarativeNetRequestKey);
  DCHECK(data);

  // The ruleset path is validated during DNRManifestHandler::Validate, and
  // hence is safe to use.
  const base::FilePath& relative_path =
      static_cast<DNRManifestData*>(data)->ruleset.relative_path;
  return extension.path().Append(relative_path);
}

}  // namespace declarative_net_request
}  // namespace extensions
