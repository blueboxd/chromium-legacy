// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_KEYED_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class TFLiteExperimentKeyedService;
class Profile;

// LazyInstance that owns all TFLiteExperimentKeyedService and associates them
// with Profiles.
class TFLiteExperimentKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the TFLiteExperimentService for the profile.
  static TFLiteExperimentKeyedService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all TFLiteExperimentKeyedService(s).
  static TFLiteExperimentKeyedServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<TFLiteExperimentKeyedServiceFactory>;

  TFLiteExperimentKeyedServiceFactory();
  ~TFLiteExperimentKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  //  CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_KEYED_SERVICE_FACTORY_H_
