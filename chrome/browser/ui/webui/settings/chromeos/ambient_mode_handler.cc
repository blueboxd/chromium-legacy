// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"

#include "ash/public/cpp/ambient/photo_controller.h"
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
  if (topic_source_.has_value())
    SendTopicSource();
}

void AmbientModeHandler::HandleInitialized(const base::ListValue* args) {
  CHECK(args);
  CHECK(args->empty());

  AllowJavascript();
  GetSettings();
}

void AmbientModeHandler::HandleTopicSourceSelectedChanged(
    const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  int topic_source;
  CHECK(base::StringToInt(args->GetList()[0].GetString(), &topic_source));
  // Check the |topic_source| has valid value.
  // TODO: 0 and 1 are enum values for Google Photos and Art gallery. Will
  // replace these values to enum after moving the enum definition to ash.
  CHECK_GE(topic_source, 0);
  CHECK_LE(topic_source, 1);

  UpdateSettings(topic_source);
}

void AmbientModeHandler::GetSettings() {
  ash::PhotoController::Get()->GetSettings(base::BindOnce(
      &AmbientModeHandler::OnGetSettings, weak_factory_.GetWeakPtr()));
}

void AmbientModeHandler::OnGetSettings(
    const base::Optional<int>& topic_source) {
  if (!topic_source.has_value()) {
    // TODO(b/152921891): Retry a small fixed number of times, then only retry
    // when user confirms in the error message dialog.
    return;
  }

  topic_source_ = topic_source;
  if (!IsJavascriptAllowed())
    return;

  SendTopicSource();
}

void AmbientModeHandler::SendTopicSource() {
  FireWebUIListener("topic-source-changed", base::Value(topic_source_.value()));
}

void AmbientModeHandler::UpdateSettings(int topic_source) {
  ash::PhotoController::Get()->UpdateSettings(
      topic_source, base::BindOnce(&AmbientModeHandler::OnUpdateSettings,
                                   weak_factory_.GetWeakPtr(), topic_source));
}

void AmbientModeHandler::OnUpdateSettings(int topic_source, bool success) {
  if (success)
    return;

  // TODO(b/152921891): Retry a small fixed number of times, then only retry
  // when user confirms in the error message dialog.
}

}  // namespace settings
}  // namespace chromeos
