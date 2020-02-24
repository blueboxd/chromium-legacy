// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/chrome_ssl_host_state_delegate_factory.h"

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/security_interstitials/content/chrome_ssl_host_state_delegate.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
ChromeSSLHostStateDelegate*
ChromeSSLHostStateDelegateFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ChromeSSLHostStateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
ChromeSSLHostStateDelegateFactory*
ChromeSSLHostStateDelegateFactory::GetInstance() {
  return base::Singleton<ChromeSSLHostStateDelegateFactory>::get();
}

ChromeSSLHostStateDelegateFactory::ChromeSSLHostStateDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeSSLHostStateDelegate",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

ChromeSSLHostStateDelegateFactory::~ChromeSSLHostStateDelegateFactory() =
    default;

KeyedService* ChromeSSLHostStateDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChromeSSLHostStateDelegate(
      context, user_prefs::UserPrefs::Get(context),
      HostContentSettingsMapFactory::GetForBrowserContext(context));
}

content::BrowserContext*
ChromeSSLHostStateDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
