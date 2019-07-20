// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"

namespace extensions {
namespace util {

bool SiteHasIsolatedStorage(const GURL& extension_site_url,
                            content::BrowserContext* context) {
  const Extension* extension = ExtensionRegistry::Get(context)->
      enabled_extensions().GetExtensionOrAppByURL(extension_site_url);

  return extension && AppIsolationInfo::HasIsolatedStorage(extension);
}

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
  return content::SiteInstance::GetSiteForURL(
      context, Extension::GetBaseURLFromExtensionId(extension_id));
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

}  // namespace util
}  // namespace extensions
