// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using ManifestV3PermissionsTest = ManifestTest;

TEST_F(ManifestV3PermissionsTest, WebRequestBlockingPermissionsTest) {
  const std::string kManifestV3NotSupported =
      "The maximum currently-supported manifest version is 2, but this is 3.  "
      "Certain features may not work as expected.";
  const std::string kPermissionRequiresV2OrLower =
      "'webRequestBlocking' requires manifest version of 2 or lower.";
  {
    // Manifest V3 extension that is not policy installed. This should trigger a
    // warning that manifest V3 is not currently supported and that the
    // webRequestBlocking permission requires a lower manifest version.
    scoped_refptr<Extension> extension(LoadAndExpectWarnings(
        "web_request_blocking_v3.json",
        {kManifestV3NotSupported, kPermissionRequiresV2OrLower},
        extensions::Manifest::Location::UNPACKED));
    ASSERT_TRUE(extension);
  }
  {
    // Manifest V3 extension that is policy extension. This should only trigger
    // a warning that manifest V3 is not supported currently.
    scoped_refptr<Extension> extension(LoadAndExpectWarnings(
        "web_request_blocking_v3.json", {kManifestV3NotSupported},
        extensions::Manifest::Location::EXTERNAL_POLICY));
    ASSERT_TRUE(extension);
  }
  {
    // Manifest V2 extension that is not policy installed. This should not
    // trigger any warnings.
    scoped_refptr<Extension> extension(
        LoadAndExpectSuccess("web_request_blocking_v2.json",
                             extensions::Manifest::Location::UNPACKED));
    ASSERT_TRUE(extension);
  }
}

}  // namespace extensions
