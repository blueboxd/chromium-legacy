// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_util.h"

#include "base/path_service.h"
#include "ios/web/public/browser_state.h"

namespace breadcrumb_persistent_storage_util {

const base::FilePath::CharType kBreadcrumbsFile[] =
    FILE_PATH_LITERAL("iOS Breadcrumbs");

base::FilePath GetBreadcrumbPersistentStorageFilePath(
    web::BrowserState* browser_state) {
  return browser_state->GetStatePath().Append(kBreadcrumbsFile);
}

}  // namespace breadcrumb_persistent_storage_util
