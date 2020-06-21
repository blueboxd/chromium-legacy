// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_
#define ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Structs and classes related to Ambient mode Settings.

// Enumeration of the topic source, i.e. where the photos come from.
// Values need to stay in sync with the |topicSource_| in ambient_mode_page.js.
// Art gallery is a super set of art related topic sources in Backdrop service.
enum class AmbientModeTopicSource {
  kMinValue = 0,
  kGooglePhotos = kMinValue,
  kArtGallery = 1,
  kMaxValue = kArtGallery
};

// Subsettings of Art gallery.
struct ASH_PUBLIC_EXPORT ArtSetting {
  ArtSetting();
  ArtSetting(const ArtSetting&);
  ArtSetting(ArtSetting&&);
  ArtSetting& operator=(const ArtSetting&);
  ArtSetting& operator=(ArtSetting&&);
  ~ArtSetting();

  int setting_id = 0;

  // Whether the setting is enabled in the Art gallery topic source.
  bool enabled = false;

  std::string title;

  std::string description;

  std::string preview_image_url;
};

struct ASH_PUBLIC_EXPORT AmbientSettings {
  AmbientSettings();
  AmbientSettings(const AmbientSettings&);
  AmbientSettings(AmbientSettings&&);
  AmbientSettings& operator=(const AmbientSettings&);
  AmbientSettings& operator=(AmbientSettings&&);
  ~AmbientSettings();

  AmbientModeTopicSource topic_source;

  // Only a subset Settings of Art gallery.
  std::vector<ArtSetting> art_settings;

  // Only selected album.
  std::vector<std::string> selected_album_ids;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_
