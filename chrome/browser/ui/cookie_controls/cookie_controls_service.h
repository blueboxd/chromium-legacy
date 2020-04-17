// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_SERVICE_H_
#define CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_SERVICE_H_

#include <memory>

#include "base/observer_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/gurl.h"

class Profile;

namespace base {
class Value;
}  // namespace base

namespace policy {
class PolicyChangeRegistrar;
}

// // Handles the global state for cookie settings changes and observation.
class CookieControlsService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnThirdPartyCookieBlockingPrefChanged() {}
    virtual void OnThirdPartyCookieBlockingPolicyChanged() {}
  };

  ~CookieControlsService() override;

  void Init();
  void Shutdown() override;

  void HandleCookieControlsToggleChanged(bool checked);
  // Whether cookie controls UI should be hidden in incognito ntp.
  bool ShouldHideCookieControlsUI();
  // Whether cookie controls should appear enforced.
  bool ShouldEnforceCookieControls();
  CookieControlsEnforcement GetCookieControlsEnforcement();
  bool GetToggleCheckedValue();

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }
  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

 private:
  friend class CookieControlsServiceFactory;

  // Use |CookieControlsServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  explicit CookieControlsService(Profile* profile);

  void OnThirdPartyCookieBlockingPrefChanged();
  void OnThirdPartyCookieBlockingPolicyChanged(const base::Value* previous,
                                               const base::Value* current);

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<policy::PolicyChangeRegistrar> policy_registrar_;
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(CookieControlsService);
};

#endif  // CHROME_BROWSER_UI_COOKIE_CONTROLS_COOKIE_CONTROLS_SERVICE_H_
