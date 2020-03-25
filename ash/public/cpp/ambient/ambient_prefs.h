// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
namespace ambient {
namespace prefs {

// Enumeration of the topic source, i.e. where the photos come from.
// Values need to stay in sync with the |topicSource_| in ambient_mode_page.js.
// Art gallery is a super set of art related topic sources in Backdrop service.
// These values are registered in prefs.
// Entries should not be renumbered and numeric values should never be reused.
// Only append to this enum is allowed if more source will be added.
enum class TopicSource {
  kGooglePhotos = 0,
  kArtGallery = 1,
  kMaxValue = kArtGallery,
};

// A GUID for backdrop client.
ASH_PUBLIC_EXPORT extern const char kAmbientBackdropClientId[];

// Boolean pref for whether ambient mode is enabled.
ASH_PUBLIC_EXPORT extern const char kAmbientModeEnabled[];

// Integer pref for ambient topic source, value is one of TopicSource.
ASH_PUBLIC_EXPORT extern const char kAmbientModeTopicSource[];

}  // namespace prefs
}  // namespace ambient
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
