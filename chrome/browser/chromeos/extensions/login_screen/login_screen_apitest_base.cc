// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"

#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace {

// The extension's code can be found in
// chrome/test/data/extensions/api_test/login_screen_apis/
const char kExtensionId[] = "oclffehlkdgibkainkilopaalpdobkan";
const char kExtensionUpdateManifestPath[] =
    "/extensions/api_test/login_screen_apis/update_manifest.xml";

// The test extension will query for a test to run using this message.
const char kWaitingForTestName[] = "Waiting for test name";

}  // namespace

namespace chromeos {

LoginScreenApitestBase::LoginScreenApitestBase(version_info::Channel channel)
    : SigninProfileExtensionsPolicyTestBase(channel),
      extension_id_(kExtensionId),
      extension_update_manifest_path_(kExtensionUpdateManifestPath) {}

LoginScreenApitestBase::~LoginScreenApitestBase() = default;

void LoginScreenApitestBase::SetUpExtensionAndRunTest(
    const std::string& testName) {
  extensions::ResultCatcher catcher;

  ExtensionTestMessageListener listener(kWaitingForTestName,
                                        /*will_reply=*/true);

  AddExtensionForForceInstallation(extension_id_,
                                   extension_update_manifest_path_);

  ASSERT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(testName);

  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace chromeos
