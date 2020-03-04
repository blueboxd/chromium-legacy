// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"
#include <tuple>

namespace feed {

bool Equal(const feedwire::ContentId& a, const feedwire::ContentId& b) {
  return a.content_domain() == b.content_domain() && a.id() == b.id() &&
         a.table() == b.table();
}

bool CompareContentId(const feedwire::ContentId& a,
                      const feedwire::ContentId& b) {
  const int a_id = a.id();  // tie() needs l-values
  const int b_id = b.id();
  return std::tie(a.content_domain(), a_id, a.table()) <
         std::tie(b.content_domain(), b_id, b.table());
}

}  // namespace feed
