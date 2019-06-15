// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dark_mode_handler.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/native_theme/native_theme.h"

DarkModeHandler::~DarkModeHandler() {}

// static
void DarkModeHandler::Initialize(content::WebUI* web_ui,
                                 content::WebUIDataSource* source) {
  InitializeInternal(web_ui, source, ui::NativeTheme::GetInstanceForNativeUi(),
                     Profile::FromWebUI(web_ui));
}

// static
void DarkModeHandler::InitializeInternal(content::WebUI* web_ui,
                                         content::WebUIDataSource* source,
                                         ui::NativeTheme* theme,
                                         Profile* profile) {
  auto handler = base::WrapUnique(new DarkModeHandler(theme, profile));
  source->AddLocalizedStrings(*handler->GetDataSourceUpdate());
  handler->source_name_ = source->GetSource();
  web_ui->AddMessageHandler(std::move(handler));
}

DarkModeHandler::DarkModeHandler(ui::NativeTheme* theme, Profile* profile)
    : profile_(profile),
      dark_mode_observer_(
          theme,
          base::BindRepeating(&DarkModeHandler::OnDarkModeChanged,
                              base::Unretained(this))) {
  dark_mode_observer_.Start();
}

void DarkModeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "observeDarkMode",
      base::BindRepeating(&DarkModeHandler::HandleObserveDarkMode,
                          base::Unretained(this)));
}

void DarkModeHandler::HandleObserveDarkMode(const base::ListValue* /*args*/) {
  AllowJavascript();
}

bool DarkModeHandler::UseDarkMode() const {
  return base::FeatureList::IsEnabled(features::kWebUIDarkMode) &&
         dark_mode_observer_.InDarkMode();
}

std::unique_ptr<base::DictionaryValue> DarkModeHandler::GetDataSourceUpdate()
    const {
  auto update = std::make_unique<base::DictionaryValue>();
  bool use_dark_mode = UseDarkMode();
  update->SetKey("dark", base::Value(use_dark_mode ? "dark" : ""));
  return update;
}

void DarkModeHandler::OnDarkModeChanged(bool /*dark_mode*/) {
  content::WebUIDataSource::Update(profile_, source_name_,
                                   GetDataSourceUpdate());
  if (IsJavascriptAllowed())
    FireWebUIListener("dark-mode-changed", base::Value(UseDarkMode()));
}
