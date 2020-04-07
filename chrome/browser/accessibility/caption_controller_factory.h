// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace captions {

class CaptionController;

// Factory to get or create an instance of CaptionController from a Profile.
class CaptionControllerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CaptionController* GetForProfile(Profile* profile);

  static CaptionController* GetForProfileIfExists(Profile* profile);

  static CaptionControllerFactory* GetInstance();

 private:
  friend base::NoDestructor<CaptionControllerFactory>;

  CaptionControllerFactory();
  ~CaptionControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_CONTROLLER_FACTORY_H_
