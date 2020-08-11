// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/media_string_view.h"

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace ash {

using MediaStringViewTest = AmbientAshTestBase;

TEST_F(MediaStringViewTest, ShowMediaTitleAndArtist) {
  ShowAmbientScreen();

  // Sets metadata for current session.
  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");
  metadata.artist = base::ASCIIToUTF16("artist");

  SimulateMediaMetadataChanged(metadata);

  const base::string16 expected_text =
      base::UTF8ToUTF16("\u266A title \u00B7 artist");
  EXPECT_EQ(GetMediaStringView()->GetText(), expected_text);
}

TEST_F(MediaStringViewTest, ShowWhenMediaIsPlaying) {
  ShowAmbientScreen();
  EXPECT_FALSE(GetMediaStringView()->GetVisible());

  // Sets media playstate for the current session.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_TRUE(GetMediaStringView()->GetVisible());
}

TEST_F(MediaStringViewTest, DoNotShowWhenMediaIsPaused) {
  ShowAmbientScreen();
  EXPECT_FALSE(GetMediaStringView()->GetVisible());

  // Sets media playstate for the current session.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_TRUE(GetMediaStringView()->GetVisible());

  // Simulates the ongoing media paused.
  SimulateMediaPlaybackStateChanged(
      media_session::mojom::MediaPlaybackState::kPaused);
  EXPECT_FALSE(GetMediaStringView()->GetVisible());
}

}  // namespace ash
