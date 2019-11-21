// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"

#include <vector>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"

namespace extensions {
namespace {
constexpr char kInvalidId[] = "Invalid id";
constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
}  // namespace

class WebstorePrivateExtensionInstallRequestBase : public ExtensionApiUnittest {
 public:
  using ExtensionInstallStatus = api::webstore_private::ExtensionInstallStatus;
  WebstorePrivateExtensionInstallRequestBase() = default;

  std::string GenerateArgs(const char* id) {
    return base::StringPrintf(R"(["%s"])", id);
  }

  scoped_refptr<const Extension> CreateExtension(const ExtensionId& id) {
    return ExtensionBuilder("extension").SetID(id).Build();
  }

  void VerifyResponse(const ExtensionInstallStatus& expected_response,
                      const base::Value* actual_response) {
    ASSERT_TRUE(actual_response->is_list());
    const auto& actual_list = actual_response->GetList();
    ASSERT_EQ(1u, actual_list.size());
    ASSERT_TRUE(actual_list[0].is_string());
    EXPECT_EQ(ToString(expected_response), actual_list[0].GetString());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebstorePrivateExtensionInstallRequestBase);
};

class WebstorePrivateGetExtensionStatusTest
    : public WebstorePrivateExtensionInstallRequestBase {};

TEST_F(WebstorePrivateGetExtensionStatusTest, InvalidExtensionId) {
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  EXPECT_EQ(kInvalidId,
            RunFunctionAndReturnError(function.get(),
                                      GenerateArgs("invalid-extension-id")));
}

TEST_F(WebstorePrivateGetExtensionStatusTest, ExtensionEnabled) {
  ExtensionRegistry::Get(profile())->AddEnabled(CreateExtension(kExtensionId));
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  std::unique_ptr<base::Value> response =
      RunFunctionAndReturnValue(function.get(), GenerateArgs(kExtensionId));
  VerifyResponse(ExtensionInstallStatus::EXTENSION_INSTALL_STATUS_ENABLED,
                 response.get());
}

class WebstorePrivateRequestExtensionTest
    : public WebstorePrivateExtensionInstallRequestBase {
 public:
  void SetUp() override {
    WebstorePrivateExtensionInstallRequestBase::SetUp();
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kCloudExtensionRequestEnabled,
        std::make_unique<base::Value>(true));
    VerifyPendingList({});
  }

  void SetPendingList(const std::vector<ExtensionId>& ids) {
    base::Value::ListStorage id_values;
    for (const auto& id : ids)
      id_values.push_back(base::Value(id));
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kCloudExtensionRequestIds,
        std::make_unique<base::Value>(std::move(id_values)));
  }

  void VerifyPendingList(
      const std::vector<ExtensionId>& expected_pending_list) {
    const base::span<const base::Value> actual_pending_list =
        profile()
            ->GetPrefs()
            ->GetList(prefs::kCloudExtensionRequestIds)
            ->GetList();
    ASSERT_EQ(expected_pending_list.size(), actual_pending_list.size());
    for (unsigned int i = 0; i < expected_pending_list.size(); i++)
      EXPECT_EQ(expected_pending_list[i], actual_pending_list[i].GetString());
  }
};

TEST_F(WebstorePrivateRequestExtensionTest, InvalidExtensionId) {
  auto function =
      base::MakeRefCounted<WebstorePrivateRequestExtensionFunction>();
  EXPECT_EQ(kInvalidId,
            RunFunctionAndReturnError(function.get(),
                                      GenerateArgs("invalid-extension-id")));
  VerifyPendingList({});
}

TEST_F(WebstorePrivateRequestExtensionTest, UnrequestableExtension) {
  ExtensionRegistry::Get(profile())->AddEnabled(CreateExtension(kExtensionId));
  auto function =
      base::MakeRefCounted<WebstorePrivateRequestExtensionFunction>();
  std::unique_ptr<base::Value> response =
      RunFunctionAndReturnValue(function.get(), GenerateArgs(kExtensionId));
  VerifyResponse(ExtensionInstallStatus::EXTENSION_INSTALL_STATUS_ENABLED,
                 response.get());
  VerifyPendingList({});
}

TEST_F(WebstorePrivateRequestExtensionTest, AlreadyPendingExtension) {
  SetPendingList({kExtensionId});
  VerifyPendingList({kExtensionId});
  auto function =
      base::MakeRefCounted<WebstorePrivateRequestExtensionFunction>();
  std::unique_ptr<base::Value> response =
      RunFunctionAndReturnValue(function.get(), GenerateArgs(kExtensionId));
  VerifyResponse(
      ExtensionInstallStatus::EXTENSION_INSTALL_STATUS_REQUEST_PENDING,
      response.get());
  VerifyPendingList({kExtensionId});
}

TEST_F(WebstorePrivateRequestExtensionTest, RequestExtension) {
  auto function =
      base::MakeRefCounted<WebstorePrivateRequestExtensionFunction>();
  std::unique_ptr<base::Value> response =
      RunFunctionAndReturnValue(function.get(), GenerateArgs(kExtensionId));
  VerifyResponse(
      ExtensionInstallStatus::EXTENSION_INSTALL_STATUS_REQUEST_PENDING,
      response.get());
  VerifyPendingList({kExtensionId});
}

}  // namespace extensions
