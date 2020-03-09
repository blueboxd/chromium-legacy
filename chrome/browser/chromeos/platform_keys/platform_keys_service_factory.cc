// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace platform_keys {

// static
PlatformKeysService* PlatformKeysServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PlatformKeysService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PlatformKeysServiceFactory* PlatformKeysServiceFactory::GetInstance() {
  return base::Singleton<PlatformKeysServiceFactory>::get();
}

PlatformKeysServiceFactory::PlatformKeysServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PlatformKeysService",
          BrowserContextDependencyManager::GetInstance()) {}

PlatformKeysServiceFactory::~PlatformKeysServiceFactory() = default;

KeyedService* PlatformKeysServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PlatformKeysServiceImpl(context);
}

content::BrowserContext* PlatformKeysServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace platform_keys
}  // namespace chromeos
