// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tflite_experiment/tflite_experiment_keyed_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tflite_experiment/tflite_experiment_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
TFLiteExperimentKeyedService*
TFLiteExperimentKeyedServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TFLiteExperimentKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TFLiteExperimentKeyedServiceFactory*
TFLiteExperimentKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<TFLiteExperimentKeyedServiceFactory> factory;
  return factory.get();
}

TFLiteExperimentKeyedServiceFactory::TFLiteExperimentKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "TFLiteExperimentKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

TFLiteExperimentKeyedServiceFactory::~TFLiteExperimentKeyedServiceFactory() =
    default;

KeyedService* TFLiteExperimentKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new TFLiteExperimentKeyedService(context);
}
