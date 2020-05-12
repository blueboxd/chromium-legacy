// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_ANDROID_FEATURES_H_
#define COMPONENTS_PAGE_INFO_ANDROID_FEATURES_H_

namespace base {
struct Feature;
}  // namespace base

namespace page_info {

// Enables the second version of the Page Info View.
extern const base::Feature kPageInfoV2;

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_ANDROID_FEATURES_H_