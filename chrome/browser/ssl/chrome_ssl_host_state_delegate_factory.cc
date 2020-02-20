// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/security_interstitials/content/chrome_ssl_host_state_delegate.h"

// static
ChromeSSLHostStateDelegate* ChromeSSLHostStateDelegateFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChromeSSLHostStateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
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
  Profile* profile = Profile::FromBrowserContext(context);
  return new ChromeSSLHostStateDelegate(
      profile, profile->GetPrefs(),
      HostContentSettingsMapFactory::GetForProfile(profile));
}

content::BrowserContext*
ChromeSSLHostStateDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
