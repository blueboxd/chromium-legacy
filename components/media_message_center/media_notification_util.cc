// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_util.h"

#include "base/strings/utf_string_conversions.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_message_center {

base::string16 GetAccessibleNameFromMetadata(
    media_session::MediaMetadata session_metadata) {
  std::vector<base::string16> text;

  if (!session_metadata.title.empty())
    text.push_back(session_metadata.title);

  if (!session_metadata.artist.empty())
    text.push_back(session_metadata.artist);

  if (!session_metadata.album.empty())
    text.push_back(session_metadata.album);

  base::string16 accessible_name =
      base::JoinString(text, base::ASCIIToUTF16(" - "));
  return accessible_name;
}

}  // namespace media_message_center
