// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace chromeos {
namespace settings {

namespace {

// Width and height of the preview image for personal album.
constexpr int kBannerWidth = 512;
constexpr int kBannderHeight = 512;

ash::AmbientModeTopicSource ExtractTopicSource(const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1U);
  ash::AmbientModeTopicSource topic_source =
      static_cast<ash::AmbientModeTopicSource>(args->GetList()[0].GetInt());
  // Check the |topic_source| has valid value.
  CHECK_GE(topic_source, ash::AmbientModeTopicSource::kMinValue);
  CHECK_LE(topic_source, ash::AmbientModeTopicSource::kMaxValue);
  return topic_source;
}

}  // namespace

AmbientModeHandler::AmbientModeHandler() = default;

AmbientModeHandler::~AmbientModeHandler() = default;

void AmbientModeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onAmbientModePageReady",
      base::BindRepeating(&AmbientModeHandler::HandleInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setSelectedTopicSource",
      base::BindRepeating(&AmbientModeHandler::HandleSetSelectedTopicSource,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestPhotosContainers",
      base::BindRepeating(&AmbientModeHandler::RequestPhotosContainers,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setSelectedPhotosContainers",
      base::BindRepeating(
          &AmbientModeHandler::HandleSetSelectedPhotosContainers,
          base::Unretained(this)));
}

void AmbientModeHandler::OnJavascriptAllowed() {
  GetSettings();

  // Prefetch albums.
  FetchPersonalAlbums();
}

void AmbientModeHandler::OnJavascriptDisallowed() {
  weak_factory_.InvalidateWeakPtrs();
}

void AmbientModeHandler::HandleInitialized(const base::ListValue* args) {
  CHECK(args);
  CHECK(args->empty());

  init_from_ambient_mode_page_ = true;
  AllowJavascript();
}

void AmbientModeHandler::RequestPhotosContainers(const base::ListValue* args) {
  // TODO(b/159747583): Handle deep linking to ambientMode/photos page.
  // For now it will not load the page contents if visited directly.
  if (!init_from_ambient_mode_page_)
    return;

  ash::AmbientModeTopicSource topic_source = ExtractTopicSource(args);
  DCHECK_EQ(topic_source, settings_->topic_source);

  if (topic_source == ash::AmbientModeTopicSource::kGooglePhotos) {
    FetchPersonalAlbums();
  }
  SendPhotosContainers();
}

void AmbientModeHandler::HandleSetSelectedTopicSource(
    const base::ListValue* args) {
  ash::AmbientModeTopicSource topic_source = ExtractTopicSource(args);
  settings_->topic_source = topic_source;
  UpdateSettings();
}

void AmbientModeHandler::HandleSetSelectedPhotosContainers(
    const base::ListValue* args) {
  switch (settings_->topic_source) {
    case ash::AmbientModeTopicSource::kGooglePhotos:
      // For Google Photos, we will populate the |selected_album_ids| with IDs
      // of selected albums.
      settings_->selected_album_ids.clear();
      for (const auto& value : args->GetList()) {
        std::string name = value.GetString();
        auto it = std::find_if(
            personal_albums_.albums.begin(), personal_albums_.albums.end(),
            [name](const auto& album) { return album.album_name == name; });
        CHECK(it != personal_albums_.albums.end());
        settings_->selected_album_ids.emplace_back(it->album_id);
      }
      break;
    case ash::AmbientModeTopicSource::kArtGallery:
      // For Art gallery, we set the corresponding setting to be enabled or not
      // based on the selections.
      for (auto& art_setting : settings_->art_settings) {
        std::string title = art_setting.title;
        auto it = std::find_if(
            args->GetList().begin(), args->GetList().end(),
            [title](const auto& value) { return value.GetString() == title; });
        const bool checked = it != args->GetList().end();
        art_setting.enabled = checked;
      }
      break;
  }

  UpdateSettings();
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

void AmbientModeHandler::SendPhotosContainers() {
  DCHECK(settings_);

  base::Value dictionary(base::Value::Type::DICTIONARY);
  base::Value containers(base::Value::Type::LIST);
  switch (settings_->topic_source) {
    case ash::AmbientModeTopicSource::kGooglePhotos:
      for (const auto& album : personal_albums_.albums) {
        base::Value value(base::Value::Type::DICTIONARY);
        value.SetKey("title", base::Value(album.album_name));
        value.SetKey("checked",
                     base::Value(base::Contains(settings_->selected_album_ids,
                                                album.album_id)));
        containers.Append(std::move(value));
      }
      break;
    case ash::AmbientModeTopicSource::kArtGallery:
      for (const auto& setting : settings_->art_settings) {
        base::Value value(base::Value::Type::DICTIONARY);
        value.SetKey("title", base::Value(setting.title));
        value.SetKey("checked", base::Value(setting.enabled));
        containers.Append(std::move(value));
      }
      break;
  }

  dictionary.SetKey("topicSource",
                    base::Value(static_cast<int>(settings_->topic_source)));
  dictionary.SetKey("topicContainers", std::move(containers));
  FireWebUIListener("photos-containers-changed", std::move(dictionary));
}

void AmbientModeHandler::UpdateSettings() {
  DCHECK(settings_);
  ash::AmbientBackendController::Get()->UpdateSettings(
      *settings_, base::BindOnce(&AmbientModeHandler::OnUpdateSettings,
                                 weak_factory_.GetWeakPtr()));
}

void AmbientModeHandler::OnUpdateSettings(bool success) {
  if (success)
    return;

  // TODO(b/152921891): Retry a small fixed number of times, then only retry
  // when user confirms in the error message dialog.
}

void AmbientModeHandler::FetchPersonalAlbums() {
  // TODO: Add a helper function to get all the albums.
  ash::AmbientBackendController::Get()->FetchPersonalAlbums(
      kBannerWidth, kBannderHeight, /*num_albums=*/100, /*resume_token=*/"",
      base::BindOnce(&AmbientModeHandler::OnPersonalAlbumsFetched,
                     weak_factory_.GetWeakPtr()));
}

void AmbientModeHandler::OnPersonalAlbumsFetched(
    ash::PersonalAlbums personal_albums) {
  personal_albums_ = std::move(personal_albums);

  // If the |topic_source| is not |kGooglePhotos|, no need to refresh the
  // photos subpage.
  // |settings_| could be null because we call GetSettings() and
  // FetchPersonalAlbums() in OnJavascriptAllowed(). |settings_| is populated by
  // OnGetSettings(), which could be called later. The purpose to call
  // FetchPersonalAlbums() is to prefetch albums, which takes several seconds,
  // This improves the experience when we click into the ambientMode/photos page
  // to show the albums list faster.
  if (settings_ &&
      settings_->topic_source == ash::AmbientModeTopicSource::kGooglePhotos) {
    SendPhotosContainers();
  }
}

}  // namespace settings
}  // namespace chromeos
