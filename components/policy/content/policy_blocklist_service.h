// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"

namespace safe_search_api {
class URLChecker;
}  // namespace safe_search_api

// PolicyBlocklistService and PolicyBlocklistFactory provide a way for
// us to access URLBlocklistManager, a policy block list service based on
// the Preference Service. The URLBlocklistManager responds to permission
// changes and is per-Profile.
class PolicyBlocklistService : public KeyedService {
 public:
  using CheckSafeSearchCallback = base::OnceCallback<void(bool is_safe)>;

  PolicyBlocklistService(
      content::BrowserContext* browser_context,
      std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager);
  ~PolicyBlocklistService() override;

  policy::URLBlocklist::URLBlocklistState GetURLBlocklistState(
      const GURL& url) const;

  // Starts a call to the Safe Search API for the given URL to determine whether
  // the URL is "safe" (not porn). Returns whether |callback| was run
  // synchronously.
  bool CheckSafeSearchURL(const GURL& url, CheckSafeSearchCallback callback);

  // Creates a SafeSearch URLChecker using a given URLLoaderFactory for testing.
  void SetSafeSearchURLCheckerForTest(
      std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker);

 private:
  content::BrowserContext* const browser_context_;
  std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager_;
  std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker_;

  DISALLOW_COPY_AND_ASSIGN(PolicyBlocklistService);
};

class PolicyBlocklistFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PolicyBlocklistFactory* GetInstance();
  static PolicyBlocklistService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  PolicyBlocklistFactory();
  ~PolicyBlocklistFactory() override;
  friend struct base::DefaultSingletonTraits<PolicyBlocklistFactory>;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PolicyBlocklistFactory);
};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_SERVICE_H_
