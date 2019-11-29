// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2MintTokenFlow.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::StrictMock;

namespace {

static const char kValidTokenResponse[] =
    "{"
    "  \"token\": \"at1\","
    "  \"issueAdvice\": \"Auto\","
    "  \"expiresIn\": \"3600\""
    "}";
static const char kTokenResponseNoAccessToken[] =
    "{"
    "  \"issueAdvice\": \"Auto\""
    "}";

static const char kValidIssueAdviceResponse[] =
    "{"
    "  \"issueAdvice\": \"consent\","
    "  \"consent\": {"
    "    \"oauthClient\": {"
    "      \"name\": \"Test app\","
    "      \"iconUri\": \"\","
    "      \"developerEmail\": \"munjal@chromium.org\""
    "    },"
    "    \"scopes\": ["
    "      {"
    "        \"description\": \"Manage your calendars\","
    "        \"detail\": \"\nView and manage your calendars\n\""
    "      },"
    "      {"
    "        \"description\": \"Manage your documents\","
    "        \"detail\": \"\nView your documents\nUpload new documents\n\""
    "      }"
    "    ]"
    "  }"
    "}";

static const char kIssueAdviceResponseNoDescription[] =
    "{"
    "  \"issueAdvice\": \"consent\","
    "  \"consent\": {"
    "    \"oauthClient\": {"
    "      \"name\": \"Test app\","
    "      \"iconUri\": \"\","
    "      \"developerEmail\": \"munjal@chromium.org\""
    "    },"
    "    \"scopes\": ["
    "      {"
    "        \"description\": \"Manage your calendars\","
    "        \"detail\": \"\nView and manage your calendars\n\""
    "      },"
    "      {"
    "        \"detail\": \"\nView your documents\nUpload new documents\n\""
    "      }"
    "    ]"
    "  }"
    "}";

static const char kIssueAdviceResponseNoDetail[] =
    "{"
    "  \"issueAdvice\": \"consent\","
    "  \"consent\": {"
    "    \"oauthClient\": {"
    "      \"name\": \"Test app\","
    "      \"iconUri\": \"\","
    "      \"developerEmail\": \"munjal@chromium.org\""
    "    },"
    "    \"scopes\": ["
    "      {"
    "        \"description\": \"Manage your calendars\","
    "        \"detail\": \"\nView and manage your calendars\n\""
    "      },"
    "      {"
    "        \"description\": \"Manage your documents\""
    "      }"
    "    ]"
    "  }"
    "}";

static const char kValidRemoteConsentResponse[] =
    "{"
    "  \"issueAdvice\": \"remoteConsent\","
    "  \"resolutionData\": {"
    "    \"resolutionApproach\": \"resolveInBrowser\","
    "    \"resolutionUrl\": \"https://test.com/consent?param=value\","
    "    \"browserCookies\": ["
    "      {"
    "        \"name\": \"test_name\","
    "        \"value\": \"test_value\","
    "        \"domain\": \"test.com\","
    "        \"path\": \"/\","
    "        \"maxAgeSeconds\": \"60\","
    "        \"isSecure\": false,"
    "        \"isHttpOnly\": true,"
    "        \"sameSite\": \"none\""
    "      },"
    "      {"
    "        \"name\": \"test_name2\","
    "        \"value\": \"test_value2\","
    "        \"domain\": \"test.com\""
    "      }"
    "    ]"
    "  }"
    "}";

static const char kInvalidRemoteConsentResponse[] =
    "{"
    "  \"issueAdvice\": \"remoteConsent\","
    "  \"resolutionData\": {"
    "    \"resolutionApproach\": \"resolveInBrowser\""
    "  }"
    "}";

std::vector<std::string> CreateTestScopes() {
  std::vector<std::string> scopes;
  scopes.push_back("http://scope1");
  scopes.push_back("http://scope2");
  return scopes;
}

static IssueAdviceInfo CreateIssueAdvice() {
  IssueAdviceInfo ia;
  IssueAdviceInfoEntry e1;
  e1.description = base::ASCIIToUTF16("Manage your calendars");
  e1.details.push_back(base::ASCIIToUTF16("View and manage your calendars"));
  ia.push_back(e1);
  IssueAdviceInfoEntry e2;
  e2.description = base::ASCIIToUTF16("Manage your documents");
  e2.details.push_back(base::ASCIIToUTF16("View your documents"));
  e2.details.push_back(base::ASCIIToUTF16("Upload new documents"));
  ia.push_back(e2);
  return ia;
}

static RemoteConsentResolutionData CreateRemoteConsentResolutionData() {
  RemoteConsentResolutionData resolution_data;
  resolution_data.url = GURL("https://test.com/consent?param=value");
  resolution_data.cookies.push_back(net::CanonicalCookie::CreateSanitizedCookie(
      resolution_data.url, "test_name", "test_value", "test.com", "/",
      base::Time(), base::Time(), base::Time(), false, true,
      net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_DEFAULT));
  resolution_data.cookies.push_back(net::CanonicalCookie::CreateSanitizedCookie(
      resolution_data.url, "test_name2", "test_value2", "test.com", "/",
      base::Time(), base::Time(), base::Time(), false, false,
      net::CookieSameSite::UNSPECIFIED, net::COOKIE_PRIORITY_DEFAULT));
  return resolution_data;
}

class MockDelegate : public OAuth2MintTokenFlow::Delegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  MOCK_METHOD2(OnMintTokenSuccess, void(const std::string& access_token,
                                        int time_to_live));
  MOCK_METHOD1(OnIssueAdviceSuccess,
               void (const IssueAdviceInfo& issue_advice));
  MOCK_METHOD1(OnRemoteConsentSuccess,
               void(const RemoteConsentResolutionData& resolution_data));
  MOCK_METHOD1(OnMintTokenFailure,
               void(const GoogleServiceAuthError& error));
};

class MockMintTokenFlow : public OAuth2MintTokenFlow {
 public:
  explicit MockMintTokenFlow(MockDelegate* delegate,
                             const OAuth2MintTokenFlow::Parameters& parameters)
      : OAuth2MintTokenFlow(delegate, parameters) {}
  ~MockMintTokenFlow() override {}

  MOCK_METHOD0(CreateAccessTokenFetcher,
               std::unique_ptr<OAuth2AccessTokenFetcher>());
};

}  // namespace

class OAuth2MintTokenFlowTest : public testing::Test {
 public:
  OAuth2MintTokenFlowTest() {}
  ~OAuth2MintTokenFlowTest() override {}

 protected:
  void CreateFlow(OAuth2MintTokenFlow::Mode mode) {
    return CreateFlow(&delegate_, mode, "");
  }

  void CreateFlowWithDeviceId(const std::string& device_id) {
    return CreateFlow(&delegate_, OAuth2MintTokenFlow::MODE_ISSUE_ADVICE,
                      device_id);
  }

  void CreateFlow(MockDelegate* delegate,
                  OAuth2MintTokenFlow::Mode mode,
                  const std::string& device_id) {
    std::string ext_id = "ext1";
    std::string client_id = "client1";
    std::vector<std::string> scopes(CreateTestScopes());
    flow_ = std::make_unique<MockMintTokenFlow>(
        delegate, OAuth2MintTokenFlow::Parameters(ext_id, client_id, scopes,
                                                  device_id, mode));
  }

  // Helper to parse the given string to base::Value.
  static std::unique_ptr<base::Value> ParseJson(const std::string& str) {
    base::Optional<base::Value> value = base::JSONReader::Read(str);
    EXPECT_TRUE(value.has_value());
    EXPECT_TRUE(value->is_dict());
    return std::make_unique<base::Value>(std::move(*value));
  }

  std::unique_ptr<MockMintTokenFlow> flow_;
  StrictMock<MockDelegate> delegate_;
};

TEST_F(OAuth2MintTokenFlowTest, CreateApiCallBody) {
  {  // Issue advice mode.
    CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
    std::string body = flow_->CreateApiCallBody();
    std::string expected_body(
        "force=false"
        "&response_type=none"
        "&scope=http://scope1+http://scope2"
        "&client_id=client1"
        "&origin=ext1");
    EXPECT_EQ(expected_body, body);
  }
  {  // Record grant mode.
    CreateFlow(OAuth2MintTokenFlow::MODE_RECORD_GRANT);
    std::string body = flow_->CreateApiCallBody();
    std::string expected_body(
        "force=true"
        "&response_type=none"
        "&scope=http://scope1+http://scope2"
        "&client_id=client1"
        "&origin=ext1");
    EXPECT_EQ(expected_body, body);
  }
  {  // Mint token no force mode.
    CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
    std::string body = flow_->CreateApiCallBody();
    std::string expected_body(
        "force=false"
        "&response_type=token"
        "&scope=http://scope1+http://scope2"
        "&client_id=client1"
        "&origin=ext1");
    EXPECT_EQ(expected_body, body);
  }
  {  // Mint token force mode.
    CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE);
    std::string body = flow_->CreateApiCallBody();
    std::string expected_body(
        "force=true"
        "&response_type=token"
        "&scope=http://scope1+http://scope2"
        "&client_id=client1"
        "&origin=ext1");
    EXPECT_EQ(expected_body, body);
  }
  {  // Mint token with device_id.
    CreateFlowWithDeviceId("device_id1");
    std::string body = flow_->CreateApiCallBody();
    std::string expected_body(
        "force=false"
        "&response_type=none"
        "&scope=http://scope1+http://scope2"
        "&client_id=client1"
        "&origin=ext1"
        "&device_id=device_id1"
        "&device_type=chrome"
        "&lib_ver=extension");
    EXPECT_EQ(expected_body, body);
  }
}

TEST_F(OAuth2MintTokenFlowTest, ParseMintTokenResponse) {
  {  // Access token missing.
    std::unique_ptr<base::Value> json = ParseJson(kTokenResponseNoAccessToken);
    std::string at;
    int ttl;
    EXPECT_FALSE(OAuth2MintTokenFlow::ParseMintTokenResponse(json.get(), &at,
                                                             &ttl));
    EXPECT_TRUE(at.empty());
  }
  {  // All good.
    std::unique_ptr<base::Value> json = ParseJson(kValidTokenResponse);
    std::string at;
    int ttl;
    EXPECT_TRUE(OAuth2MintTokenFlow::ParseMintTokenResponse(json.get(), &at,
                                                            &ttl));
    EXPECT_EQ("at1", at);
    EXPECT_EQ(3600, ttl);
  }
}

TEST_F(OAuth2MintTokenFlowTest, ParseIssueAdviceResponse) {
  {  // Description missing.
    std::unique_ptr<base::Value> json =
        ParseJson(kIssueAdviceResponseNoDescription);
    IssueAdviceInfo ia;
    EXPECT_FALSE(OAuth2MintTokenFlow::ParseIssueAdviceResponse(
        json.get(), &ia));
    EXPECT_TRUE(ia.empty());
  }
  {  // Detail missing.
    std::unique_ptr<base::Value> json = ParseJson(kIssueAdviceResponseNoDetail);
    IssueAdviceInfo ia;
    EXPECT_FALSE(OAuth2MintTokenFlow::ParseIssueAdviceResponse(
        json.get(), &ia));
    EXPECT_TRUE(ia.empty());
  }
  {  // All good.
    std::unique_ptr<base::Value> json = ParseJson(kValidIssueAdviceResponse);
    IssueAdviceInfo ia;
    EXPECT_TRUE(OAuth2MintTokenFlow::ParseIssueAdviceResponse(
        json.get(), &ia));
    IssueAdviceInfo ia_expected(CreateIssueAdvice());
    EXPECT_EQ(ia_expected, ia);
  }
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  RemoteConsentResolutionData resolution_data;
  ASSERT_TRUE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  RemoteConsentResolutionData expected_resolution_data =
      CreateRemoteConsentResolutionData();
  EXPECT_EQ(resolution_data, expected_resolution_data);
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_EmptyCookies) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  json->FindListPath("resolutionData.browserCookies")->GetList().clear();
  RemoteConsentResolutionData resolution_data;
  EXPECT_TRUE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  RemoteConsentResolutionData expected_resolution_data =
      CreateRemoteConsentResolutionData();
  expected_resolution_data.cookies.clear();
  EXPECT_EQ(resolution_data, expected_resolution_data);
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoCookies) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  EXPECT_TRUE(json->RemovePath("resolutionData.browserCookies"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_TRUE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  RemoteConsentResolutionData expected_resolution_data =
      CreateRemoteConsentResolutionData();
  expected_resolution_data.cookies.clear();
  EXPECT_EQ(resolution_data, expected_resolution_data);
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoResolutionData) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  EXPECT_TRUE(json->RemoveKey("resolutionData"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoUrl) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  EXPECT_TRUE(json->RemovePath("resolutionData.resolutionUrl"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadUrl) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  EXPECT_TRUE(json->SetStringPath("resolutionData.resolutionUrl", "not-a-url"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_NoApproach) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  EXPECT_TRUE(json->RemovePath("resolutionData.resolutionApproach"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadApproach) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  EXPECT_TRUE(
      json->SetStringPath("resolutionData.resolutionApproach", "badApproach"));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest,
       ParseRemoteConsentResponse_BadCookie_MissingRequiredField) {
  static const char* kRequiredFields[] = {"name", "value", "domain"};
  for (const auto* required_field : kRequiredFields) {
    std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
    base::Value::ListStorage& cookies =
        json->FindListPath("resolutionData.browserCookies")->GetList();
    EXPECT_TRUE(cookies[0].RemoveKey(required_field));
    RemoteConsentResolutionData resolution_data;
    EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
        json.get(), &resolution_data));
    EXPECT_TRUE(resolution_data.url.is_empty());
    EXPECT_TRUE(resolution_data.cookies.empty());
  }
}

TEST_F(OAuth2MintTokenFlowTest,
       ParseRemoteConsentResponse_MissingCookieOptionalField) {
  static const char* kOptionalFields[] = {"path", "maxAgeSeconds", "isSecure",
                                          "isHttpOnly", "sameSite"};
  for (const auto* optional_field : kOptionalFields) {
    std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
    base::Value::ListStorage& cookies =
        json->FindListPath("resolutionData.browserCookies")->GetList();
    EXPECT_TRUE(cookies[0].RemoveKey(optional_field));
    RemoteConsentResolutionData resolution_data;
    EXPECT_TRUE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
        json.get(), &resolution_data));
    RemoteConsentResolutionData expected_resolution_data =
        CreateRemoteConsentResolutionData();
    EXPECT_EQ(resolution_data, expected_resolution_data);
  }
}

TEST_F(OAuth2MintTokenFlowTest,
       ParseRemoteConsentResponse_BadCookie_BadMaxAge) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  base::Value::ListStorage& cookies =
      json->FindListPath("resolutionData.browserCookies")->GetList();
  cookies[0].SetStringKey("maxAgeSeconds", "not-a-number");
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ParseRemoteConsentResponse_BadCookieList) {
  std::unique_ptr<base::Value> json = ParseJson(kValidRemoteConsentResponse);
  base::Value::ListStorage& cookies =
      json->FindListPath("resolutionData.browserCookies")->GetList();
  cookies.push_back(base::Value(42));
  RemoteConsentResolutionData resolution_data;
  EXPECT_FALSE(OAuth2MintTokenFlow::ParseRemoteConsentResponse(
      json.get(), &resolution_data));
  EXPECT_TRUE(resolution_data.url.is_empty());
  EXPECT_TRUE(resolution_data.cookies.empty());
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallSuccess) {
  network::mojom::URLResponseHeadPtr head_200 =
      network::CreateURLResponseHead(net::HTTP_OK);

  {  // No body.
    CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
    EXPECT_CALL(delegate_, OnMintTokenFailure(_));
    flow_->ProcessApiCallSuccess(head_200.get(), nullptr);
  }
  {  // Bad json.
    CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
    EXPECT_CALL(delegate_, OnMintTokenFailure(_));
    flow_->ProcessApiCallSuccess(head_200.get(),
                                 std::make_unique<std::string>("foo"));
  }
  {  // Valid json: no access token.
    CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
    EXPECT_CALL(delegate_, OnMintTokenFailure(_));
    flow_->ProcessApiCallSuccess(
        head_200.get(),
        std::make_unique<std::string>(kTokenResponseNoAccessToken));
  }
  {  // Valid json: good token response.
    CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
    EXPECT_CALL(delegate_, OnMintTokenSuccess("at1", 3600));
    flow_->ProcessApiCallSuccess(
        head_200.get(), std::make_unique<std::string>(kValidTokenResponse));
  }
  {  // Valid json: no description.
    CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
    EXPECT_CALL(delegate_, OnMintTokenFailure(_));
    flow_->ProcessApiCallSuccess(
        head_200.get(),
        std::make_unique<std::string>(kIssueAdviceResponseNoDescription));
  }
  {  // Valid json: no detail.
    CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
    EXPECT_CALL(delegate_, OnMintTokenFailure(_));
    flow_->ProcessApiCallSuccess(
        head_200.get(),
        std::make_unique<std::string>(kIssueAdviceResponseNoDetail));
  }
  {  // Valid json: good issue advice response.
    CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
    IssueAdviceInfo ia(CreateIssueAdvice());
    EXPECT_CALL(delegate_, OnIssueAdviceSuccess(ia));
    flow_->ProcessApiCallSuccess(
        head_200.get(),
        std::make_unique<std::string>(kValidIssueAdviceResponse));
  }
  {  // Valid json: remote consent response.
    CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
    RemoteConsentResolutionData resolution_data =
        CreateRemoteConsentResolutionData();
    EXPECT_CALL(delegate_, OnRemoteConsentSuccess(Eq(ByRef(resolution_data))));
    flow_->ProcessApiCallSuccess(
        head_200.get(),
        std::make_unique<std::string>(kValidRemoteConsentResponse));
  }
  {  // Valid json: bad remote consent response, fallback to issue advice.
    CreateFlow(OAuth2MintTokenFlow::MODE_ISSUE_ADVICE);
    IssueAdviceInfo empty_info;
    EXPECT_CALL(delegate_, OnIssueAdviceSuccess(empty_info));
    flow_->ProcessApiCallSuccess(
        head_200.get(),
        std::make_unique<std::string>(kInvalidRemoteConsentResponse));
  }
}

TEST_F(OAuth2MintTokenFlowTest, ProcessApiCallFailure) {
  network::mojom::URLResponseHead head;
  {  // Null delegate should work fine.
    CreateFlow(nullptr, OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE, "");
    flow_->ProcessApiCallFailure(net::ERR_FAILED, &head, nullptr);
  }

  {  // Non-null delegate.
    CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
    EXPECT_CALL(delegate_, OnMintTokenFailure(_));
    flow_->ProcessApiCallFailure(net::ERR_FAILED, &head, nullptr);
  }

  {  // Null head might happen.
    CreateFlow(OAuth2MintTokenFlow::MODE_MINT_TOKEN_NO_FORCE);
    EXPECT_CALL(delegate_, OnMintTokenFailure(_));
    flow_->ProcessApiCallFailure(net::ERR_FAILED, nullptr, nullptr);
  }
}
