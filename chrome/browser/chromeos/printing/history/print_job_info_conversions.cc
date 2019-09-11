// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_info_conversions.h"

namespace chromeos {

namespace {

printing::proto::PrintSettings_ColorMode ColorModelToProto(
    ::printing::ColorModel color) {
  return ::printing::IsColorModelSelected(color)
             ? printing::proto::PrintSettings_ColorMode_COLOR
             : printing::proto::PrintSettings_ColorMode_BLACK_AND_WHITE;
}

printing::proto::PrintSettings_DuplexMode DuplexModeToProto(
    ::printing::DuplexMode duplex) {
  switch (duplex) {
    case ::printing::DuplexMode::SIMPLEX:
      return printing::proto::PrintSettings_DuplexMode_ONE_SIDED;
    case ::printing::DuplexMode::LONG_EDGE:
      return printing::proto::PrintSettings_DuplexMode_TWO_SIDED_LONG_EDGE;
    case ::printing::DuplexMode::SHORT_EDGE:
      return printing::proto::PrintSettings_DuplexMode_TWO_SIDED_SHORT_EDGE;
    default:
      NOTREACHED();
  }
  return printing::proto::PrintSettings_DuplexMode_ONE_SIDED;
}

printing::proto::MediaSize RequestedMediaToProto(
    const ::printing::PrintSettings::RequestedMedia& media) {
  printing::proto::MediaSize media_size_proto;
  media_size_proto.set_width(media.size_microns.width());
  media_size_proto.set_height(media.size_microns.height());
  media_size_proto.set_vendor_id(media.vendor_id);
  return media_size_proto;
}

}  // namespace

printing::proto::PrintSettings PrintSettingsToProto(
    const ::printing::PrintSettings& settings) {
  printing::proto::PrintSettings settings_proto;
  settings_proto.set_color(ColorModelToProto(settings.color()));
  settings_proto.set_duplex(DuplexModeToProto(settings.duplex_mode()));
  *settings_proto.mutable_media_size() =
      RequestedMediaToProto(settings.requested_media());
  settings_proto.set_copies(settings.copies());
  return settings_proto;
}

}  // namespace chromeos
