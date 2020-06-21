// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace chromeos {
namespace settings {

AmbientModeHandler::AmbientModeHandler() = default;

AmbientModeHandler::~AmbientModeHandler() = default;

void AmbientModeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onAmbientModePageReady",
      base::BindRepeating(&AmbientModeHandler::HandleInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "onTopicSourceSelectedChanged",
      base::BindRepeating(&AmbientModeHandler::HandleTopicSourceSelectedChanged,
                          base::Unretained(this)));
}

void AmbientModeHandler::OnJavascriptAllowed() {
  GetSettings();
}

void AmbientModeHandler::OnJavascriptDisallowed() {
  weak_factory_.InvalidateWeakPtrs();
}

void AmbientModeHandler::HandleInitialized(const base::ListValue* args) {
  CHECK(args);
  CHECK(args->empty());

  AllowJavascript();
}

void AmbientModeHandler::HandleTopicSourceSelectedChanged(
    const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  int topic_source;
  CHECK(base::StringToInt(args->GetList()[0].GetString(), &topic_source));
  // Check the |topic_source| has valid value.
  CHECK_GE(topic_source,
           static_cast<int>(ash::AmbientModeTopicSource::kMinValue));
  CHECK_LE(topic_source,
           static_cast<int>(ash::AmbientModeTopicSource::kMaxValue));

  UpdateSettings(static_cast<ash::AmbientModeTopicSource>(topic_source));
}

void AmbientModeHandler::GetSettings() {
  ash::AmbientBackendController::Get()->GetSettings(base::BindOnce(
      &AmbientModeHandler::OnGetSettings, weak_factory_.GetWeakPtr()));
}

void AmbientModeHandler::OnGetSettings(
    const base::Optional<ash::AmbientSettings>& settings) {
  if (!settings) {
    // TODO(b/152921891): Retry a small fixed number of times, then only retry
    // when user confirms in the error message dialog.
    return;
  }

  settings_ = settings;
  SendTopicSource();
}

void AmbientModeHandler::SendTopicSource() {
  DCHECK(settings_);
  FireWebUIListener("topic-source-changed",
                    base::Value(static_cast<int>(settings_->topic_source)));
}

void AmbientModeHandler::UpdateSettings(
    ash::AmbientModeTopicSource topic_source) {
  DCHECK(settings_);
  settings_->topic_source = topic_source;
  ash::AmbientBackendController::Get()->UpdateSettings(
      *settings_, base::BindOnce(&AmbientModeHandler::OnUpdateSettings,
                                 weak_factory_.GetWeakPtr(), topic_source));
}

void AmbientModeHandler::OnUpdateSettings(
    ash::AmbientModeTopicSource topic_source,
    bool success) {
  if (success)
    return;

  // TODO(b/152921891): Retry a small fixed number of times, then only retry
  // when user confirms in the error message dialog.
}

}  // namespace settings
}  // namespace chromeos
