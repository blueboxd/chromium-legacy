// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/mature_extension_checker.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "url/gurl.h"

namespace {

std::string ValueToString(const base::Value& value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

}  // namespace

namespace extensions {

MatureExtensionChecker::MatureExtensionChecker(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

MatureExtensionChecker::~MatureExtensionChecker() = default;

bool MatureExtensionChecker::HasRequestForExtension(
    const std::string& extension_id) const {
  return base::Contains(pending_webstore_fetches_, extension_id);
}

bool MatureExtensionChecker::HasDataForExtension(
    const std::string& extension_id) const {
  return base::Contains(mature_extensions_map_, extension_id);
}

void MatureExtensionChecker::CheckMatureDataForExtension(
    const std::string& extension_id) {
  // Check asynchronously if the extension is rated mature.
  auto webstore_data_fetcher =
      std::make_unique<WebstoreDataFetcher>(this, GURL(), extension_id);
  webstore_data_fetcher->set_max_auto_retries(3);
  webstore_data_fetcher->Start(
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
  // Keep track of the request and ensure the WebstoreDataFetcher is not
  // deleted.
  pending_webstore_fetches_.emplace(extension_id,
                                    std::move(webstore_data_fetcher));
}

bool MatureExtensionChecker::IsExtensionMature(
    const std::string& extension_id) const {
  std::map<std::string, bool>::const_iterator it =
      mature_extensions_map_.find(extension_id);
  if (it != mature_extensions_map_.end())
    return it->second;
  return false;
}

void MatureExtensionChecker::MarkExtensionMatureForTesting(
    const std::string& extension_id,
    bool maturity_rating) {
  auto webstore_data = std::make_unique<base::DictionaryValue>();
  webstore_data->SetString(kIdKey, extension_id);
  webstore_data->SetBoolean(kFamilyUnsafeKey, maturity_rating);
  OnWebstoreResponseParseSuccess(extension_id, std::move(webstore_data));
}

void MatureExtensionChecker::ChangeExtensionStateIfNecessary(
    const std::string& extension_id) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  // If the extension is not installed, do nothing.
  if (!extension)
    return;

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  extensions::ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();

  bool is_mature = IsExtensionMature(extension_id);
  if (is_mature) {
    service->DisableExtension(extension_id,
                              disable_reason::DISABLE_BLOCKED_MATURE);
  } else {
    extension_prefs->RemoveDisableReason(
        extension_id, extensions::disable_reason::DISABLE_BLOCKED_MATURE);
    if (extension_prefs->GetDisableReasons(extension_id) ==
        extensions::disable_reason::DISABLE_NONE) {
      service->EnableExtension(extension_id);
    }
  }
}

void MatureExtensionChecker::OnWebstoreRequestFailure(
    const std::string& extension_id) {
  LOG(ERROR) << "WebstoreDataFetcher request failure for extension id "
             << extension_id;
  ExtensionRequestMap::iterator it =
      pending_webstore_fetches_.find(extension_id);
  DCHECK(it != pending_webstore_fetches_.end());
  mature_extensions_map_[extension_id] = false;
  pending_webstore_fetches_.erase(it);
}

void MatureExtensionChecker::OnWebstoreResponseParseSuccess(
    const std::string& extension_id,
    std::unique_ptr<base::DictionaryValue> webstore_data) {
  ExtensionRequestMap::iterator it =
      pending_webstore_fetches_.find(extension_id);
  DCHECK(it != pending_webstore_fetches_.end());

  bool family_unsafe = false;
  if (!webstore_data->GetBoolean(kFamilyUnsafeKey, &family_unsafe)) {
    LOG(ERROR) << "Webstore response error (" << kFamilyUnsafeKey
               << "): " << ValueToString(*webstore_data.get());
    OnWebstoreResponseParseFailure(extension_id, kInvalidWebstoreResponseError);
    return;
  }
  mature_extensions_map_[extension_id] = family_unsafe;
  ChangeExtensionStateIfNecessary(extension_id);
  pending_webstore_fetches_.erase(it);
}

void MatureExtensionChecker::OnWebstoreResponseParseFailure(
    const std::string& extension_id,
    const std::string& error) {
  LOG(ERROR) << "WebstoreDataFetcher response parse failure for extension id "
             << extension_id << ": " << error;
  ExtensionRequestMap::iterator it =
      pending_webstore_fetches_.find(extension_id);
  DCHECK(it != pending_webstore_fetches_.end());
  mature_extensions_map_[extension_id] = false;
  pending_webstore_fetches_.erase(it);
}

}  // namespace extensions
