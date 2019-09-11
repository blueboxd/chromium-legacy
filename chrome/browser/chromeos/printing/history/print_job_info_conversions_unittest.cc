// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_info_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr int kWidth = 297000;
constexpr int kHeight = 420000;

constexpr char kVendorId[] = "iso_a3_297x420mm";

}  // namespace

TEST(PrintJobInfoConversionsTest, PrintSettingsToProto) {
  ::printing::PrintSettings settings;
  settings.set_color(::printing::ColorModel::COLOR);
  settings.set_duplex_mode(::printing::DuplexMode::LONG_EDGE);
  ::printing::PrintSettings::RequestedMedia media;
  media.size_microns = gfx::Size(kWidth, kHeight);
  media.vendor_id = kVendorId;
  settings.set_requested_media(media);
  settings.set_copies(2);

  printing::proto::PrintSettings settings_proto =
      PrintSettingsToProto(settings);
  const printing::proto::MediaSize& media_size = settings_proto.media_size();

  EXPECT_EQ(printing::proto::PrintSettings_ColorMode_COLOR,
            settings_proto.color());
  EXPECT_EQ(printing::proto::PrintSettings_DuplexMode_TWO_SIDED_LONG_EDGE,
            settings_proto.duplex());
  EXPECT_EQ(kWidth, media_size.width());
  EXPECT_EQ(kHeight, media_size.height());
  EXPECT_EQ(kVendorId, media_size.vendor_id());
  EXPECT_EQ(2, settings_proto.copies());
}

}  // namespace chromeos
