// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_PROFILE_H_
#define WEBLAYER_PUBLIC_PROFILE_H_

#include <algorithm>
#include <string>

namespace weblayer {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplBrowsingDataType
enum class BrowsingDataType {
  COOKIES_AND_SITE_DATA = 0,
  CACHE = 1,
};

class Profile {
 public:
  // Pass an empty |name| for an in-memory profile.
  // Otherwise, |name| should contain only alphanumeric characters and
  // underscore.
  static std::unique_ptr<Profile> Create(const std::string& name);

  virtual ~Profile() {}

  virtual void ClearBrowsingData(
      const std::vector<BrowsingDataType>& data_types,
      base::Time from_time,
      base::Time to_time,
      base::OnceClosure callback) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_PROFILE_H_
