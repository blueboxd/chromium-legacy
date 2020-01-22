// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"

namespace autofill {

class StrikeDatabase;

// Singleton that owns all StrikeDatabases and associates them with
// ios::ChromeBrowserState.
class StrikeDatabaseFactory : public BrowserStateKeyedServiceFactory {
 public:
  static StrikeDatabase* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static StrikeDatabaseFactory* GetInstance();

 private:
  friend class base::NoDestructor<StrikeDatabaseFactory>;

  StrikeDatabaseFactory();
  ~StrikeDatabaseFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(StrikeDatabaseFactory);
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_
