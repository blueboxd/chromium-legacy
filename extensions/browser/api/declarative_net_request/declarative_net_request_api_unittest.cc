// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"

#include "base/memory/ref_counted.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {
namespace {

using DeclarativeNetRequestApiUnittest = ApiUnitTest;

TEST_F(DeclarativeNetRequestApiUnittest, SetActionCountAsBadgeText) {
  // Set the current channel to trunk.
  ScopedCurrentChannel scoped_channel(version_info::Channel::UNKNOWN);
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser_context());

  // Preference should be false by default.
  EXPECT_FALSE(
      extension_prefs->GetDNRUseActionCountAsBadgeText(extension()->id()));

  auto function = base::MakeRefCounted<
      DeclarativeNetRequestSetActionCountAsBadgeTextFunction>();
  function->set_extension(extension());

  api_test_utils::RunFunction(function.get(), "[true]", browser_context());
  extension_prefs = ExtensionPrefs::Get(browser_context());

  EXPECT_TRUE(
      extension_prefs->GetDNRUseActionCountAsBadgeText(extension()->id()));
}

}  // namespace
}  // namespace extensions
