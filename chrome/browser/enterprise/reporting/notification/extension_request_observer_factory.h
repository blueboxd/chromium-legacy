// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_NOTIFICATION_EXTENSION_REQUEST_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_NOTIFICATION_EXTENSION_REQUEST_OBSERVER_FACTORY_H_

#include <map>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

namespace enterprise_reporting {

class ExtensionRequestObserver;

// Factory class for ExtensionRequestObserver. It creates
// ExtensionRequestObserver for each Profile.
class ExtensionRequestObserverFactory : public ProfileManagerObserver {
 public:
  ExtensionRequestObserverFactory();
  ExtensionRequestObserverFactory(const ExtensionRequestObserverFactory&) =
      delete;
  ExtensionRequestObserverFactory& operator=(
      const ExtensionRequestObserverFactory&) = delete;
  ~ExtensionRequestObserverFactory() override;

  ExtensionRequestObserver* GetObserverByProfileForTesting(Profile* profile);
  int GetNumberOfObserversForTesting();

 private:
  // ProfileManagerObserver
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

  std::map<Profile*, std::unique_ptr<ExtensionRequestObserver>, ProfileCompare>
      observers_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_NOTIFICATION_EXTENSION_REQUEST_OBSERVER_FACTORY_H_
