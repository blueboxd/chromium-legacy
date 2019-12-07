// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace util {

bool CanBeIncognitoEnabled(const Extension* extension) {
  return IncognitoInfo::IsIncognitoAllowed(extension) &&
         (!extension->is_platform_app() ||
          extension->location() == Manifest::COMPONENT);
}

bool IsIncognitoEnabled(const std::string& extension_id,
                        content::BrowserContext* context) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, ExtensionRegistry::ENABLED);
  if (extension) {
    if (!CanBeIncognitoEnabled(extension))
      return false;
    // If this is an existing component extension we always allow it to
    // work in incognito mode.
    if (Manifest::IsComponentLocation(extension->location()))
      return true;
    if (extension->is_login_screen_extension())
      return true;
  }
  return ExtensionPrefs::Get(context)->IsIncognitoEnabled(extension_id);
}

bool CanCrossIncognito(const Extension* extension,
                       content::BrowserContext* context) {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  CHECK(extension);
  return IsIncognitoEnabled(extension->id(), context) &&
         !IncognitoInfo::IsSplitMode(extension);
}

GURL GetSiteForExtensionId(const std::string& extension_id,
                           content::BrowserContext* context) {
  GURL site = content::SiteInstance::GetSiteForURL(
      context, Extension::GetBaseURLFromExtensionId(extension_id));
  DCHECK_EQ(extension_id, site.host());
  return site;
}

const std::string& GetPartitionDomainForExtension(const Extension* extension) {
  // Extensions use their own ID for a partition domain.
  return extension->id();
}

content::StoragePartition* GetStoragePartitionForExtensionId(
    const std::string& extension_id,
    content::BrowserContext* browser_context) {
  GURL site_url = content::SiteInstance::GetSiteForURL(
      browser_context, Extension::GetBaseURLFromExtensionId(extension_id));
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(browser_context,
                                                          site_url);
  return storage_partition;
}

// This function is security sensitive. Bugs could cause problems that break
// restrictions on local file access or NaCl's validation caching. If you modify
// this function, please get a security review from a NaCl person.
bool MapUrlToLocalFilePath(const ExtensionSet* extensions,
                           const GURL& file_url,
                           bool use_blocking_api,
                           base::FilePath* file_path) {
  // Check that the URL is recognized by the extension system.
  const Extension* extension = extensions->GetExtensionOrAppByURL(file_url);
  if (!extension)
    return false;

  // This is a short-cut which avoids calling a blocking file operation
  // (GetFilePath()), so that this can be called on the non blocking threads. It
  // only handles a subset of the urls.
  if (!use_blocking_api) {
    if (file_url.SchemeIs(extensions::kExtensionScheme)) {
      std::string path = file_url.path();
      base::TrimString(path, "/", &path);  // Remove first slash
      *file_path = extension->path().AppendASCII(path);
      return true;
    }
    return false;
  }

  std::string path = file_url.path();
  ExtensionResource resource;

  if (SharedModuleInfo::IsImportedPath(path)) {
    // Check if this is a valid path that is imported for this extension.
    std::string new_extension_id;
    std::string new_relative_path;
    SharedModuleInfo::ParseImportedPath(path, &new_extension_id,
                                        &new_relative_path);
    const Extension* new_extension = extensions->GetByID(new_extension_id);
    if (!new_extension)
      return false;

    if (!SharedModuleInfo::ImportsExtensionById(extension, new_extension_id))
      return false;

    resource = new_extension->GetResource(new_relative_path);
  } else {
    // Check that the URL references a resource in the extension.
    resource = extension->GetResource(path);
  }

  if (resource.empty())
    return false;

  // GetFilePath is a blocking function call.
  const base::FilePath resource_file_path = resource.GetFilePath();
  if (resource_file_path.empty())
    return false;

  *file_path = resource_file_path;
  return true;
}

bool CanWithholdPermissionsFromExtension(const Extension& extension) {
  return CanWithholdPermissionsFromExtension(
      extension.id(), extension.GetType(), extension.location());
}

bool CanWithholdPermissionsFromExtension(const ExtensionId& extension_id,
                                         Manifest::Type type,
                                         Manifest::Location location) {
  // Some extensions must retain privilege to all requested host permissions.
  // Specifically, extensions that don't show up in chrome:extensions (where
  // withheld permissions couldn't be granted), extensions that are part of
  // chrome or corporate policy, and extensions that are whitelisted to script
  // everywhere must always have permission to run on a page.
  return Extension::ShouldDisplayInExtensionSettings(type, location) &&
         !Manifest::IsPolicyLocation(location) &&
         !Manifest::IsComponentLocation(location) &&
         !PermissionsData::CanExecuteScriptEverywhere(extension_id, location);
}

}  // namespace util
}  // namespace extensions
