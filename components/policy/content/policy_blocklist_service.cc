// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_service.h"

#include <utility>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_util.h"
#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"
#include "components/safe_search_api/url_checker.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"

namespace {

// Calls the PolicyBlocklistService callback with the result of the Safe Search
// API check.
void OnCheckURLDone(PolicyBlocklistService::CheckSafeSearchCallback callback,
                    const GURL& /* url */,
                    safe_search_api::Classification classification,
                    bool /* uncertain */) {
  std::move(callback).Run(classification ==
                          safe_search_api::Classification::SAFE);
}

}  // namespace

PolicyBlocklistService::PolicyBlocklistService(
    content::BrowserContext* browser_context,
    std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager)
    : browser_context_(browser_context),
      url_blocklist_manager_(std::move(url_blocklist_manager)) {}

PolicyBlocklistService::~PolicyBlocklistService() = default;

policy::URLBlocklist::URLBlocklistState
PolicyBlocklistService::GetURLBlocklistState(const GURL& url) const {
  return url_blocklist_manager_->GetURLBlocklistState(url);
}

bool PolicyBlocklistService::CheckSafeSearchURL(
    const GURL& url,
    CheckSafeSearchCallback callback) {
  if (!safe_search_url_checker_) {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("policy_blacklist_service", R"(
          semantics {
            sender: "Cloud Policy"
            description:
              "Checks whether a given URL (or set of URLs) is considered safe "
              "by Google SafeSearch."
            trigger:
              "If the policy for safe sites is enabled, this is sent for every "
              "top-level navigation if the result isn't already cached."
            data: "URL to be checked."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature is off by default and cannot be controlled in "
              "settings."
            chrome_policy {
              SafeSitesFilterBehavior {
                SafeSitesFilterBehavior: 0
              }
            }
          })");

    safe_search_url_checker_ = std::make_unique<safe_search_api::URLChecker>(
        std::make_unique<safe_search_api::SafeSearchURLCheckerClient>(
            content::BrowserContext::GetDefaultStoragePartition(
                browser_context_)
                ->GetURLLoaderFactoryForBrowserProcess(),
            traffic_annotation));
  }

  return safe_search_url_checker_->CheckURL(
      policy::url_util::Normalize(url),
      base::BindOnce(&OnCheckURLDone, std::move(callback)));
}

void PolicyBlocklistService::SetSafeSearchURLCheckerForTest(
    std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker) {
  safe_search_url_checker_ = std::move(safe_search_url_checker);
}

// static
PolicyBlocklistFactory* PolicyBlocklistFactory::GetInstance() {
  return base::Singleton<PolicyBlocklistFactory>::get();
}

// static
PolicyBlocklistService* PolicyBlocklistFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PolicyBlocklistService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PolicyBlocklistFactory::PolicyBlocklistFactory()
    : BrowserContextKeyedServiceFactory(
          "PolicyBlocklist",
          BrowserContextDependencyManager::GetInstance()) {}

PolicyBlocklistFactory::~PolicyBlocklistFactory() = default;

KeyedService* PolicyBlocklistFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  PrefService* pref_service = user_prefs::UserPrefs::Get(context);
  auto url_blocklist_manager =
      std::make_unique<policy::URLBlocklistManager>(pref_service);
  return new PolicyBlocklistService(context, std::move(url_blocklist_manager));
}

content::BrowserContext* PolicyBlocklistFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
