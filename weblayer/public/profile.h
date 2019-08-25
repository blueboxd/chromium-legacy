// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_PROFILE_H_
#define WEBLAYER_PUBLIC_PROFILE_H_

#include <algorithm>
#include <string>

#include "base/files/file_path.h"

namespace weblayer {

class Profile {
 public:
  // Pass an empty |path| for an in-memory profile.
  static std::unique_ptr<Profile> Create(const base::FilePath& path);

  virtual ~Profile() {}

  // TODO: add lots of parameters to control what gets deleted and which range.
  virtual void ClearBrowsingData() = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_PROFILE_H_
