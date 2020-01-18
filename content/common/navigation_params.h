// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_H_

#include <string>
#include "content/common/navigation_params.mojom-forward.h"

namespace content {

// Struct keeping track of the Javascript SourceLocation that triggered the
// navigation. This is initialized based on information from Blink at the start
// of navigation, and passed back to Blink when the navigation commits.
struct CONTENT_EXPORT SourceLocation {
  SourceLocation();
  SourceLocation(const std::string& url,
                 unsigned int line_number,
                 unsigned int column_number);
  ~SourceLocation();
  std::string url;
  unsigned int line_number = 0;
  unsigned int column_number = 0;
};

CONTENT_EXPORT mojom::CommonNavigationParamsPtr CreateCommonNavigationParams();
CONTENT_EXPORT mojom::CommitNavigationParamsPtr CreateCommitNavigationParams();
CONTENT_EXPORT mojom::InitiatorCSPInfoPtr CreateInitiatorCSPInfo();

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_H_
