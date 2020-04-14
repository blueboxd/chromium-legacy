// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/server_printers_provider_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/printing/server_printers_provider.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
ServerPrintersProviderFactory* ServerPrintersProviderFactory::GetInstance() {
  static base::NoDestructor<ServerPrintersProviderFactory> instance;
  return instance.get();
}

// static
ServerPrintersProvider* ServerPrintersProviderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ServerPrintersProvider*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ServerPrintersProviderFactory::ServerPrintersProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "ServerPrintersProviderFactory",
          BrowserContextDependencyManager::GetInstance()) {}

ServerPrintersProviderFactory::~ServerPrintersProviderFactory() = default;

KeyedService* ServerPrintersProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  return ServerPrintersProvider::Create(profile).release();
}

content::BrowserContext* ServerPrintersProviderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool ServerPrintersProviderFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace chromeos
