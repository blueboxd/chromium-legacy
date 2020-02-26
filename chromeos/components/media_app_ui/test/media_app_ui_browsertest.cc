// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/test/media_app_ui_browsertest.h"

#include "base/files/file_path.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"

namespace {

// // Path to the JS that is injected into the guest frame when it navigates.
constexpr base::FilePath::CharType kTestScriptPath[] = FILE_PATH_LITERAL(
    "chromeos/components/media_app_ui/test/guest_query_receiver.js");

}  // namespace

MediaAppUiBrowserTest::MediaAppUiBrowserTest()
    : SandboxedWebUiAppTestBase(chromeos::kChromeUIMediaAppURL,
                                chromeos::kChromeUIMediaAppGuestURL,
                                base::FilePath(kTestScriptPath)) {}

MediaAppUiBrowserTest::~MediaAppUiBrowserTest() = default;

// static
std::string MediaAppUiBrowserTest::AppJsTestLibrary() {
  return SandboxedWebUiAppTestBase::LoadJsTestLibrary(
      base::FilePath(kTestScriptPath));
}
