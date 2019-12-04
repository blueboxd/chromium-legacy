// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_sticky_settings.h"

#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace printing {

namespace {

const char kSettingAppState[] = "appState";

}  // namespace

// static
PrintPreviewStickySettings* PrintPreviewStickySettings::GetInstance() {
  static base::NoDestructor<PrintPreviewStickySettings> instance;
  return instance.get();
}

PrintPreviewStickySettings::PrintPreviewStickySettings() = default;

PrintPreviewStickySettings::~PrintPreviewStickySettings() = default;

const std::string* PrintPreviewStickySettings::printer_app_state() const {
  return printer_app_state_ ? &printer_app_state_.value() : nullptr;
}

void PrintPreviewStickySettings::StoreAppState(const std::string& data) {
  printer_app_state_ = base::make_optional(data);
}

void PrintPreviewStickySettings::SaveInPrefs(PrefService* prefs) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (printer_app_state_)
    dict.SetKey(kSettingAppState, base::Value(*printer_app_state_));
  prefs->Set(prefs::kPrintPreviewStickySettings, dict);
}

void PrintPreviewStickySettings::RestoreFromPrefs(PrefService* prefs) {
  const base::DictionaryValue* value =
      prefs->GetDictionary(prefs::kPrintPreviewStickySettings);
  const base::Value* app_state =
      value->FindKeyOfType(kSettingAppState, base::Value::Type::STRING);
  if (app_state)
    StoreAppState(app_state->GetString());
}

// static
void PrintPreviewStickySettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kPrintPreviewStickySettings);
}

}  // namespace printing
