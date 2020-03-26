// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/test/media_app_ui_browsertest.h"

#include "base/files/file_path.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"

namespace {

// File containing the test utility library, shared with integration tests.
constexpr base::FilePath::CharType kTestLibraryPath[] = FILE_PATH_LITERAL(
    "chromeos/components/media_app_ui/test/dom_testing_helpers.js");

// File containing the query handlers for JS unit tests.
constexpr base::FilePath::CharType kGuestQueryHandler[] = FILE_PATH_LITERAL(
    "chromeos/components/media_app_ui/test/guest_query_receiver.js");

// Test cases that run in the guest context.
constexpr base::FilePath::CharType kGuestTestCases[] = FILE_PATH_LITERAL(
    "chromeos/components/media_app_ui/test/media_app_guest_ui_browsertest.js");

}  // namespace

MediaAppUiBrowserTest::MediaAppUiBrowserTest()
    : SandboxedWebUiAppTestBase(
          chromeos::kChromeUIMediaAppURL,
          chromeos::kChromeUIMediaAppGuestURL,
          {base::FilePath(kTestLibraryPath), base::FilePath(kGuestQueryHandler),
           base::FilePath(kGuestTestCases)}) {}

MediaAppUiBrowserTest::~MediaAppUiBrowserTest() = default;

// static
std::string MediaAppUiBrowserTest::AppJsTestLibrary() {
  return SandboxedWebUiAppTestBase::LoadJsTestLibrary(
      base::FilePath(kTestLibraryPath));
}
