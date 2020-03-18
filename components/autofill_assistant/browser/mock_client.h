// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_H_

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/website_login_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockClient : public Client {
 public:
  MockClient();
  ~MockClient();

  MOCK_CONST_METHOD0(GetApiKey, std::string());
  MOCK_CONST_METHOD0(GetServerUrl, std::string());
  MOCK_CONST_METHOD0(GetLocale, std::string());
  MOCK_CONST_METHOD0(GetCountryCode, std::string());
  MOCK_CONST_METHOD0(GetDeviceContext, DeviceContext());
  MOCK_CONST_METHOD0(GetAccountEmailAddress, std::string());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_CONST_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_CONST_METHOD0(GetWebsiteLoginFetcher, WebsiteLoginFetcher*());
  MOCK_CONST_METHOD0(GetPasswordManagerClient,
                     password_manager::PasswordManagerClient*());
  MOCK_METHOD0(GetAccessTokenFetcher, AccessTokenFetcher*());
  MOCK_METHOD1(Shutdown, void(Metrics::DropOutReason reason));
  MOCK_METHOD0(AttachUI, void());
  MOCK_METHOD0(DestroyUI, void());

 private:
  std::unique_ptr<MockPersonalDataManager> mock_personal_data_manager_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_H_
