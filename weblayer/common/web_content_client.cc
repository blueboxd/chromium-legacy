// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/common/web_content_client.h"

#include "build/build_config.h"
#include "content/app/resources/grit/content_resources.h"
#include "content/app/strings/grit/content_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace weblayer {

WebContentClient::WebContentClient() {}

WebContentClient::~WebContentClient() {}

base::StringPiece WebContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* WebContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& WebContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace weblayer
