// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_helper.h"

#include <vector>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace browsing_data {

bool IsWebScheme(const std::string& scheme) {
  const std::vector<std::string>& schemes = url::GetWebStorageSchemes();
  return base::Contains(schemes, scheme);
}

bool HasWebScheme(const GURL& origin) {
  return IsWebScheme(origin.scheme());
}

}  // namespace browsing_data
