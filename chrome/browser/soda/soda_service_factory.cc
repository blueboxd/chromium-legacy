// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/soda/soda_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/soda/soda_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
soda::SodaService* SodaServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<soda::SodaService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SodaServiceFactory* SodaServiceFactory::GetInstance() {
  static base::NoDestructor<SodaServiceFactory> instance;
  return instance.get();
}

SodaServiceFactory::SodaServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SodaService",
          BrowserContextDependencyManager::GetInstance()) {}

SodaServiceFactory::~SodaServiceFactory() = default;

KeyedService* SodaServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new soda::SodaService();
}
