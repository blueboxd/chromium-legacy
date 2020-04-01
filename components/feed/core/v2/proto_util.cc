// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"

#include <tuple>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/feed/core/proto/v2/store.pb.h"

namespace feed {

std::string ContentIdString(const feedwire::ContentId& content_id) {
  return base::StrCat({content_id.content_domain(), ",",
                       base::NumberToString(content_id.type()), ",",
                       base::NumberToString(content_id.id())});
}

bool Equal(const feedwire::ContentId& a, const feedwire::ContentId& b) {
  return a.content_domain() == b.content_domain() && a.id() == b.id() &&
         a.type() == b.type();
}

bool CompareContentId(const feedwire::ContentId& a,
                      const feedwire::ContentId& b) {
  // Local variables because tie() needs l-values.
  const int a_id = a.id();
  const int b_id = b.id();
  const feedwire::ContentId::Type a_type = a.type();
  const feedwire::ContentId::Type b_type = b.type();
  return std::tie(a.content_domain(), a_id, a_type) <
         std::tie(b.content_domain(), b_id, b_type);
}

}  // namespace feed

namespace feedstore {
void SetLastAddedTime(base::Time t, feedstore::StreamData* data) {
  data->set_last_added_time_millis(
      (t - base::Time::UnixEpoch()).InMilliseconds());
}

base::Time GetLastAddedTime(const feedstore::StreamData& data) {
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromMilliseconds(data.last_added_time_millis());
}
}  // namespace feedstore
