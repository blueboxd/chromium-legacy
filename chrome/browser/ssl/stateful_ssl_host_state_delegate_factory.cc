// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"

// static
StatefulSSLHostStateDelegate*
StatefulSSLHostStateDelegateFactory::GetForProfile(Profile* profile) {
  return static_cast<StatefulSSLHostStateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
StatefulSSLHostStateDelegateFactory*
StatefulSSLHostStateDelegateFactory::GetInstance() {
  return base::Singleton<StatefulSSLHostStateDelegateFactory>::get();
}

StatefulSSLHostStateDelegateFactory::StatefulSSLHostStateDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "StatefulSSLHostStateDelegate",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

StatefulSSLHostStateDelegateFactory::~StatefulSSLHostStateDelegateFactory() =
    default;

KeyedService* StatefulSSLHostStateDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new StatefulSSLHostStateDelegate(
      profile, profile->GetPrefs(),
      HostContentSettingsMapFactory::GetForProfile(profile));
}

content::BrowserContext*
StatefulSSLHostStateDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
