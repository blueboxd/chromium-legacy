// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/extension_info.h"

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permissions_data.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

using namespace extensions;

namespace {

em::Extension_ExtensionType GetExtensionType(Manifest::Type extension_type) {
  switch (extension_type) {
    case Manifest::TYPE_UNKNOWN:
    case Manifest::TYPE_SHARED_MODULE:
      return em::Extension_ExtensionType_TYPE_UNKNOWN;
    case Manifest::TYPE_EXTENSION:
      return em::Extension_ExtensionType_TYPE_EXTENSION;
    case Manifest::TYPE_THEME:
      return em::Extension_ExtensionType_TYPE_THEME;
    case Manifest::TYPE_USER_SCRIPT:
      return em::Extension_ExtensionType_TYPE_USER_SCRIPT;
    case Manifest::TYPE_HOSTED_APP:
      return em::Extension_ExtensionType_TYPE_HOSTED_APP;
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      return em::Extension_ExtensionType_TYPE_LEGACY_PACKAGED_APP;
    case Manifest::TYPE_PLATFORM_APP:
      return em::Extension_ExtensionType_TYPE_PLATFORM_APP;
    case Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
      return em::Extension_ExtensionType_TYPE_LOGIN_SCREEN_EXTENSION;
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
      return em::Extension_ExtensionType_TYPE_UNKNOWN;
  }
}

em::Extension_InstallType GetExtensionInstallType(
    Manifest::Location extension_location) {
  switch (extension_location) {
    case Manifest::INTERNAL:
      return em::Extension_InstallType_TYPE_NORMAL;
    case Manifest::UNPACKED:
    case Manifest::COMMAND_LINE:
      return em::Extension_InstallType_TYPE_DEVELOPMENT;
    case Manifest::EXTERNAL_PREF:
    case Manifest::EXTERNAL_REGISTRY:
    case Manifest::EXTERNAL_PREF_DOWNLOAD:
      return em::Extension_InstallType_TYPE_SIDELOAD;
    case Manifest::EXTERNAL_POLICY:
    case Manifest::EXTERNAL_POLICY_DOWNLOAD:
      return em::Extension_InstallType_TYPE_ADMIN;
    case Manifest::NUM_LOCATIONS:
      NOTREACHED();
      FALLTHROUGH;
    case Manifest::INVALID_LOCATION:
    case Manifest::COMPONENT:
    case Manifest::EXTERNAL_COMPONENT:
      return em::Extension_InstallType_TYPE_OTHER;
  }
}

void AddPermission(const Extension* extension, em::Extension* extension_info) {
  for (const std::string& permission :
       extension->permissions_data()->active_permissions().GetAPIsAsStrings()) {
    extension_info->add_permissions(permission);
  }
}

void AddHostPermission(const Extension* extension,
                       em::Extension* extension_info) {
  for (const auto& url :
       extension->permissions_data()->active_permissions().explicit_hosts()) {
    extension_info->add_host_permissions(url.GetAsString());
  }
}

void AddExtensions(const ExtensionSet& extensions,
                   em::ChromeUserProfileInfo* profile_info,
                   bool enabled) {
  for (const auto& extension : extensions) {
    // Skip the component extension.
    if (!extension->ShouldExposeViaManagementAPI())
      continue;

    auto* extension_info = profile_info->add_extensions();
    extension_info->set_id(extension->id());
    extension_info->set_version(extension->VersionString());
    extension_info->set_name(extension->name());
    extension_info->set_description(extension->description());
    extension_info->set_app_type(GetExtensionType(extension->GetType()));
    extension_info->set_homepage_url(
        ManifestURL::GetHomepageURL(extension.get()).spec());
    extension_info->set_install_type(
        GetExtensionInstallType(extension->location()));
    extension_info->set_enabled(enabled);
    AddPermission(extension.get(), extension_info);
    AddHostPermission(extension.get(), extension_info);
  }
}

}  // namespace

void AppendExtensionInfoIntoProfileReport(
    Profile* profile,
    em::ChromeUserProfileInfo* profile_info) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  AddExtensions(registry->enabled_extensions(), profile_info, true);
  AddExtensions(registry->disabled_extensions(), profile_info, false);
  AddExtensions(registry->terminated_extensions(), profile_info, false);
}

}  // namespace enterprise_reporting
