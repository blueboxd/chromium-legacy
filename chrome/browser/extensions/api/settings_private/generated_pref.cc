// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/generated_pref.h"

#include "chrome/common/extensions/api/settings_private.h"

namespace settings_api = extensions::api::settings_private;

namespace extensions {
namespace settings_private {

GeneratedPref::Observer::Observer() = default;
GeneratedPref::Observer::~Observer() = default;

GeneratedPref::GeneratedPref() = default;
GeneratedPref::~GeneratedPref() = default;

void GeneratedPref::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GeneratedPref::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GeneratedPref::NotifyObservers(const std::string& pref_name) {
  for (Observer& observer : observers_)
    observer.OnGeneratedPrefChanged(pref_name);
}

/* static */
void GeneratedPref::ApplyControlledByFromPref(
    api::settings_private::PrefObject* pref_object,
    const PrefService::Preference* pref) {
  if (pref->IsManaged()) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
    return;
  }

  if (pref->IsExtensionControlled()) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_EXTENSION;
    return;
  }

  if (pref->IsManagedByCustodian()) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
    return;
  }

  NOTREACHED();
}

}  // namespace settings_private
}  // namespace extensions
