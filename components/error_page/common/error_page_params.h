// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_ERROR_COMMON_ERROR_PAGE_PARAMS_H_
#define COMPONENTS_NET_ERROR_COMMON_ERROR_PAGE_PARAMS_H_

namespace error_page {

// Optional parameters that affect the display of an error page.
//
// TODO(mmenke): Now that this is only one value, it should probably be removed.
struct ErrorPageParams {
  ErrorPageParams();
  ~ErrorPageParams();

  // Overrides whether reloading is suggested.
  bool suggest_reload;
};

}  // namespace error_page

#endif  // COMPONENTS_NET_ERROR_COMMON_ERROR_PAGE_PARAMS_H_
