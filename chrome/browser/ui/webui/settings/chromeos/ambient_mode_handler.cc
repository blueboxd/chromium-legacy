// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace chromeos {
namespace settings {

namespace {

// Width and height of the preview image for personal album.
constexpr int kBannerWidth = 512;
constexpr int kBannerHeight = 512;

// Strings for converting to and from AmbientModeTemperatureUnit enum.
constexpr char kCelsius[] = "celsius";
constexpr char kFahrenheit[] = "fahrenheit";

ash::AmbientModeTemperatureUnit ExtractTemperatureUnit(
    const base::ListValue* args) {
  auto temperature_unit = args->GetList()[0].GetString();
  if (temperature_unit == kCelsius) {
    return ash::AmbientModeTemperatureUnit::kCelsius;
  } else if (temperature_unit == kFahrenheit) {
    return ash::AmbientModeTemperatureUnit::kFahrenheit;
  }
  NOTREACHED() << "Unknown temperature unit";
  return ash::AmbientModeTemperatureUnit::kFahrenheit;
}

std::string TemperatureUnitToString(
    ash::AmbientModeTemperatureUnit temperature_unit) {
  switch (temperature_unit) {
    case ash::AmbientModeTemperatureUnit::kFahrenheit:
      return kFahrenheit;
    case ash::AmbientModeTemperatureUnit::kCelsius:
      return kCelsius;
  }
}

ash::AmbientModeTopicSource ExtractTopicSource(const base::Value& value) {
  ash::AmbientModeTopicSource topic_source =
      static_cast<ash::AmbientModeTopicSource>(value.GetInt());
  // Check the |topic_source| has valid value.
  CHECK_GE(topic_source, ash::AmbientModeTopicSource::kMinValue);
  CHECK_LE(topic_source, ash::AmbientModeTopicSource::kMaxValue);
  return topic_source;
}

ash::AmbientModeTopicSource ExtractTopicSource(const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  return ExtractTopicSource(args->GetList()[0]);
}

}  // namespace

AmbientModeHandler::AmbientModeHandler() = default;

AmbientModeHandler::~AmbientModeHandler() = default;

void AmbientModeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestSettings",
      base::BindRepeating(&AmbientModeHandler::HandleRequestSettings,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestAlbums",
      base::BindRepeating(&AmbientModeHandler::HandleRequestAlbums,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setSelectedTopicSource",
      base::BindRepeating(&AmbientModeHandler::HandleSetSelectedTopicSource,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setSelectedTemperatureUnit",
      base::BindRepeating(&AmbientModeHandler::HandleSetSelectedTemperatureUnit,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setSelectedAlbums",
      base::BindRepeating(&AmbientModeHandler::HandleSetSelectedAlbums,
                          base::Unretained(this)));
}

void AmbientModeHandler::OnJavascriptDisallowed() {
  backend_weak_factory_.InvalidateWeakPtrs();
  ui_update_weak_factory_.InvalidateWeakPtrs();
}

void AmbientModeHandler::HandleRequestSettings(const base::ListValue* args) {
  CHECK(args);
  CHECK(args->empty());

  AllowJavascript();

  // Settings subpages may have changed from ambientMode/photos to ambientMode
  // since the last time requesting the data. Abort any request in progress to
  // avoid unnecessary updating invisible subpage.
  ui_update_weak_factory_.InvalidateWeakPtrs();
  RequestSettingsAndAlbums(
      base::BindOnce(&AmbientModeHandler::OnSettingsAndAlbumsFetched,
                     ui_update_weak_factory_.GetWeakPtr(),
                     /*topic_source=*/base::nullopt));
}

void AmbientModeHandler::HandleRequestAlbums(const base::ListValue* args) {
  CHECK(args);
  CHECK_EQ(args->GetSize(), 1U);

  AllowJavascript();

  // ambientMode/photos subpages may have changed, e.g. from displaying Google
  // Photos to Art gallery, since the last time requesting the data.
  // Abort any request in progress to avoid updating incorrect contents.
  ui_update_weak_factory_.InvalidateWeakPtrs();
  RequestSettingsAndAlbums(base::BindOnce(
      &AmbientModeHandler::OnSettingsAndAlbumsFetched,
      ui_update_weak_factory_.GetWeakPtr(), ExtractTopicSource(args)));
}

void AmbientModeHandler::HandleSetSelectedTemperatureUnit(
    const base::ListValue* args) {
  DCHECK(settings_);
  CHECK_EQ(1U, args->GetSize());

  settings_->temperature_unit = ExtractTemperatureUnit(args);
  UpdateSettings();
}

void AmbientModeHandler::HandleSetSelectedTopicSource(
    const base::ListValue* args) {
  DCHECK(settings_);
  CHECK_EQ(1U, args->GetSize());

  settings_->topic_source = ExtractTopicSource(args);
  UpdateSettings();
}

void AmbientModeHandler::HandleSetSelectedAlbums(const base::ListValue* args) {
  const base::DictionaryValue* dictionary = nullptr;
  CHECK(!args->GetList().empty());
  args->GetList()[0].GetAsDictionary(&dictionary);
  CHECK(dictionary);

  const base::Value* topic_source_value = dictionary->FindKey("topicSource");
  CHECK(topic_source_value);
  ash::AmbientModeTopicSource topic_source =
      ExtractTopicSource(*topic_source_value);
  const base::Value* albums = dictionary->FindKey("albums");
  CHECK(albums);
  switch (topic_source) {
    case ash::AmbientModeTopicSource::kGooglePhotos:
      // For Google Photos, we will populate the |selected_album_ids| with IDs
      // of selected albums.
      settings_->selected_album_ids.clear();
      for (const auto& album : albums->GetList()) {
        const base::Value* album_id = album.FindKey("albumId");
        const std::string& id = album_id->GetString();
        auto it = std::find_if(
            personal_albums_.albums.begin(), personal_albums_.albums.end(),
            [&id](const auto& album) { return album.album_id == id; });
        CHECK(it != personal_albums_.albums.end());
        settings_->selected_album_ids.emplace_back(it->album_id);
      }
      break;
    case ash::AmbientModeTopicSource::kArtGallery:
      // For Art gallery, we set the corresponding setting to be enabled or not
      // based on the selections.
      for (auto& art_setting : settings_->art_settings) {
        const std::string& album_id = art_setting.album_id;
        auto it = std::find_if(
            albums->GetList().begin(), albums->GetList().end(),
            [&album_id](const auto& album) {
              return album.FindKey("albumId")->GetString() == album_id;
            });
        const bool checked = it != albums->GetList().end();
        art_setting.enabled = checked;
      }
      break;
  }

  UpdateSettings();
}

void AmbientModeHandler::SendTemperatureUnit() {
  DCHECK(settings_);
  FireWebUIListener(
      "temperature-unit-changed",
      base::Value(TemperatureUnitToString(settings_->temperature_unit)));
}

void AmbientModeHandler::SendTopicSource() {
  DCHECK(settings_);
  FireWebUIListener("topic-source-changed",
                    base::Value(static_cast<int>(settings_->topic_source)));
}

void AmbientModeHandler::SendAlbums(ash::AmbientModeTopicSource topic_source) {
  DCHECK(settings_);

  base::Value dictionary(base::Value::Type::DICTIONARY);
  base::Value albums(base::Value::Type::LIST);
  switch (topic_source) {
    case ash::AmbientModeTopicSource::kGooglePhotos:
      for (const auto& album : personal_albums_.albums) {
        base::Value value(base::Value::Type::DICTIONARY);
        value.SetKey("albumId", base::Value(album.album_id));
        value.SetKey("title", base::Value(album.album_name));
        value.SetKey("checked",
                     base::Value(base::Contains(settings_->selected_album_ids,
                                                album.album_id)));
        albums.Append(std::move(value));
      }
      break;
    case ash::AmbientModeTopicSource::kArtGallery:
      for (const auto& setting : settings_->art_settings) {
        base::Value value(base::Value::Type::DICTIONARY);
        value.SetKey("albumId", base::Value(setting.album_id));
        value.SetKey("title", base::Value(setting.title));
        value.SetKey("checked", base::Value(setting.enabled));
        albums.Append(std::move(value));
      }
      break;
  }

  dictionary.SetKey("topicSource", base::Value(static_cast<int>(topic_source)));
  dictionary.SetKey("albums", std::move(albums));
  FireWebUIListener("albums-changed", std::move(dictionary));
}

void AmbientModeHandler::UpdateSettings() {
  DCHECK(settings_);
  ash::AmbientBackendController::Get()->UpdateSettings(
      *settings_, base::BindOnce(&AmbientModeHandler::OnUpdateSettings,
                                 backend_weak_factory_.GetWeakPtr()));
}

void AmbientModeHandler::OnUpdateSettings(bool success) {
  if (success)
    return;

  // TODO(b/152921891): Retry a small fixed number of times, then only retry
  // when user confirms in the error message dialog.
}

void AmbientModeHandler::RequestSettingsAndAlbums(
    ash::AmbientBackendController::OnSettingsAndAlbumsFetchedCallback
        callback) {
  // TODO(b/161044021): Add a helper function to get all the albums. Currently
  // only load 100 latest modified albums.
  ash::AmbientBackendController::Get()->FetchSettingsAndAlbums(
      kBannerWidth, kBannerHeight, /*num_albums=*/100, std::move(callback));
}

void AmbientModeHandler::OnSettingsAndAlbumsFetched(
    base::Optional<ash::AmbientModeTopicSource> topic_source,
    const base::Optional<ash::AmbientSettings>& settings,
    ash::PersonalAlbums personal_albums) {
  // TODO(b/152921891): Retry a small fixed number of times, then only retry
  // when user confirms in the error message dialog.
  if (!settings)
    return;

  settings_ = settings;
  personal_albums_ = std::move(personal_albums);

  if (topic_source) {
    SendAlbums(*topic_source);
    return;
  }

  SendTopicSource();
  SendTemperatureUnit();
}

}  // namespace settings
}  // namespace chromeos
