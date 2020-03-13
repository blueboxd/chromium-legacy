// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/dnr_manifest_handler.h"

#include "base/files/file_path.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "tools/json_schema_compiler/util.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {

DNRManifestHandler::DNRManifestHandler() = default;
DNRManifestHandler::~DNRManifestHandler() = default;

bool DNRManifestHandler::Parse(Extension* extension, base::string16* error) {
  DCHECK(extension->manifest()->HasKey(keys::kDeclarativeNetRequestKey));
  DCHECK(IsAPIAvailable());

  if (!PermissionsParser::HasAPIPermission(
          extension, APIPermission::kDeclarativeNetRequest)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kDeclarativeNetRequestPermissionNeeded, kAPIPermission,
        keys::kDeclarativeNetRequestKey);
    return false;
  }

  const base::DictionaryValue* dict = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kDeclarativeNetRequestKey,
                                            &dict)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeNetRequestKey,
        keys::kDeclarativeNetRequestKey);
    return false;
  }

  const base::ListValue* rules_file_list = nullptr;
  if (!dict->GetList(keys::kDeclarativeRuleResourcesKey, &rules_file_list)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeRulesFileKey,
        keys::kDeclarativeNetRequestKey, keys::kDeclarativeRuleResourcesKey);
    return false;
  }

  std::vector<dnr_api::Ruleset> rulesets;
  if (!json_schema_compiler::util::PopulateArrayFromList(*rules_file_list,
                                                         &rulesets, error)) {
    return false;
  }

  // TODO(crbug.com/754526, crbug.com/953894): Extension should be able to
  // specify zero or more than one rulesets.
  if (rulesets.size() != 1u) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidDeclarativeRulesFileKey,
        keys::kDeclarativeNetRequestKey, keys::kDeclarativeRuleResourcesKey);
    return false;
  }

  // Path validation.
  ExtensionResource resource = extension->GetResource(rulesets[0].path);
  if (resource.empty() || resource.relative_path().ReferencesParent()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
        keys::kDeclarativeRuleResourcesKey, rulesets[0].path);
    return false;
  }

  DNRManifestData::RulesetInfo info;
  info.relative_path = resource.relative_path().NormalizePathSeparators();
  info.id = kMinValidStaticRulesetID;

  extension->SetManifestData(
      keys::kDeclarativeNetRequestKey,
      std::make_unique<DNRManifestData>(std::move(info)));
  return true;
}

bool DNRManifestHandler::Validate(const Extension* extension,
                                  std::string* error,
                                  std::vector<InstallWarning>* warnings) const {
  DCHECK(IsAPIAvailable());

  DNRManifestData* data = static_cast<DNRManifestData*>(
      extension->GetManifestData(manifest_keys::kDeclarativeNetRequestKey));
  DCHECK(data);

  // Check file path validity. We don't use Extension::GetResource since it
  // returns a failure if the relative path contains Windows path separators and
  // we have already normalized the path separators.
  if (!ExtensionResource::GetFilePath(
           extension->path(), data->ruleset.relative_path,
           ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT)
           .empty()) {
    return true;
  }

  *error = ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey,
      data->ruleset.relative_path.AsUTF8Unsafe());
  return false;
}

base::span<const char* const> DNRManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kDeclarativeNetRequestKey};
  return kKeys;
}

}  // namespace declarative_net_request
}  // namespace extensions
