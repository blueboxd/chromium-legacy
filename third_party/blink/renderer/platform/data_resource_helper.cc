// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/data_resource_helper.h"

#include "ui/base/resource/resource_bundle.h"

namespace blink {

String UncompressResourceAsString(int resource_id) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string uncompressed = bundle.DecompressDataResourceScaled(
      resource_id, bundle.GetMaxScaleFactor());
  return String::FromUTF8(uncompressed);
}

String UncompressResourceAsASCIIString(int resource_id) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string uncompressed = bundle.DecompressDataResourceScaled(
      resource_id, bundle.GetMaxScaleFactor());
  return String(uncompressed.c_str(), uncompressed.size());
}

Vector<char> UncompressResourceAsBinary(int resource_id) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string uncompressed = bundle.DecompressDataResourceScaled(
      resource_id, bundle.GetMaxScaleFactor());
  Vector<char> result;
  result.Append(uncompressed.data(), uncompressed.size());
  return result;
}

}  // namespace blink
