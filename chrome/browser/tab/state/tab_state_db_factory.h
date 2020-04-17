// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STATE_TAB_STATE_DB_FACTORY_H_
#define CHROME_BROWSER_TAB_STATE_TAB_STATE_DB_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;
class TabStateDB;

// Factory to create on TabStateDB per profile. Incognito is currently
// not supported and the factory will return nullptr for an incognito profile.
class TabStateDBFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Acquire instance of TabStateDBFactory
  static TabStateDBFactory* GetInstance();

  // Acquire TabStateDB - there is one per profile.
  static TabStateDB* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<TabStateDBFactory>;

  TabStateDBFactory();
  ~TabStateDBFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_TAB_STATE_TAB_STATE_DB_FACTORY_H_
