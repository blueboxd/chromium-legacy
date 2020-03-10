// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"
#include <tuple>

namespace feed {

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
