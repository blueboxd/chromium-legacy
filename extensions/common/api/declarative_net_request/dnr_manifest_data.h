// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace declarative_net_request {

// Manifest data required for the kDeclarativeNetRequestKey manifest
// key.
struct DNRManifestData : Extension::ManifestData {
  struct RulesetInfo {
    RulesetInfo();
    ~RulesetInfo();

    RulesetInfo(RulesetInfo&&);
    RulesetInfo& operator=(RulesetInfo&&);

    base::FilePath relative_path;

    // ID provided for the ruleset in the extension manifest. Uniquely
    // identifies the ruleset.
    std::string manifest_id;

    // Uniquely identifies an extension ruleset. The order of rulesets within
    // the manifest defines the order for ids. In practice, this is equal to
    // kMinValidStaticRulesetID + the index of the ruleset within |rulesets|.
    // Note: we introduce another notion of a ruleset ID in addition to
    // |manifest_id| since the id is also used as an input to preference keys
    // and indexed ruleset file paths, and integral IDs are easier to reason
    // about here. E.g. a string ID can have invalid file path characters.
    RulesetID id;

    // Whether the ruleset is enabled by default. Note that this value
    // corresponds to the one specified in the extension manifest. Extensions
    // may further dynamically toggle whether a ruleset is enabled or not.
    bool enabled = false;
  };

  explicit DNRManifestData(std::vector<RulesetInfo> ruleset);
  ~DNRManifestData() override;

  // Returns the RulesetInfo for the |extension|. For an extension, which didn't
  // specify a static ruleset, an empty vector is returned.
  static const std::vector<RulesetInfo>& GetRulesets(
      const Extension& extension);

  // Returns the |manifest_id| corresponding to the given |ruleset_id|.
  static const std::string& GetManifestID(const Extension& extension,
                                          RulesetID ruleset_id);

  // Static rulesets specified by the extension in its manifest, in the order in
  // which they were specified.
  std::vector<RulesetInfo> rulesets;

  DISALLOW_COPY_AND_ASSIGN(DNRManifestData);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_
