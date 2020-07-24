// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"

#include "ash/public/cpp/ash_features.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {

// static
HoldingSpaceKeyedServiceFactory*
HoldingSpaceKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<HoldingSpaceKeyedServiceFactory> factory;
  return factory.get();
}

HoldingSpaceKeyedServiceFactory::HoldingSpaceKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "HoldingSpaceService",
          BrowserContextDependencyManager::GetInstance()) {}

HoldingSpaceKeyedService* HoldingSpaceKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<HoldingSpaceKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

KeyedService* HoldingSpaceKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!features::IsTemporaryHoldingSpaceEnabled())
    return nullptr;

  // TODO(https://crbug.com/1107713): Support multi-profile.
  if (!chromeos::ProfileHelper::IsPrimaryProfile(
          Profile::FromBrowserContext(context))) {
    return nullptr;
  }

  auto* service = new HoldingSpaceKeyedService(context);
  service->ActivateModel();
  return service;
}

bool HoldingSpaceKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace ash
