// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_client.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/base/protobuf_http_client_test_messages.pb.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_status.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using protobufhttpclienttest::EchoRequest;
using protobufhttpclienttest::EchoResponse;

using ::base::test::RunOnceCallback;
using ::testing::_;

using MockEchoResponseCallback =
    base::MockCallback<base::OnceCallback<void(const ProtobufHttpStatus&,
                                               std::unique_ptr<EchoResponse>)>>;

constexpr char kTestServerEndpoint[] = "test.com";
constexpr char kTestRpcPath[] = "/v1/echo:echo";
constexpr char kTestFullUrl[] = "https://test.com/v1/echo:echo";
constexpr char kRequestText[] = "This is a request";
constexpr char kResponseText[] = "This is a response";
constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kFakeAccessToken[] = "fake_access_token";
constexpr char kFakeAccessTokenHeaderValue[] = "Bearer fake_access_token";

MATCHER_P(HasStatusCode, status_code, "") {
  return arg.http_status_code() == status_code;
}

MATCHER_P(HasNetError, net_error, "") {
  return arg.net_error() == net_error;
}

MATCHER(IsResponseText, "") {
  return arg->text() == kResponseText;
}

MATCHER(IsNullResponse, "") {
  return arg.get() == nullptr;
}

class MockOAuthTokenGetter : public OAuthTokenGetter {
 public:
  MOCK_METHOD1(CallWithToken, void(TokenCallback));
  MOCK_METHOD0(InvalidateCache, void());
};

std::unique_ptr<ProtobufHttpRequest> CreateDefaultTestRequest() {
  auto request =
      std::make_unique<ProtobufHttpRequest>(TRAFFIC_ANNOTATION_FOR_TESTS);
  auto request_message = std::make_unique<EchoRequest>();
  request_message->set_text(kRequestText);
  request->request_message = std::move(request_message);
  request->SetResponseCallback(
      base::DoNothing::Once<const ProtobufHttpStatus&,
                            std::unique_ptr<EchoResponse>>());
  request->path = kTestRpcPath;
  return request;
}

std::string CreateDefaultResponseContent() {
  EchoResponse response;
  response.set_text(kResponseText);
  return response.SerializeAsString();
}

}  // namespace

class ProtobufHttpClientTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockOAuthTokenGetter mock_token_getter_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  ProtobufHttpClient client_{kTestServerEndpoint, &mock_token_getter_,
                             test_shared_loader_factory_};
};

TEST_F(ProtobufHttpClientTest, SendRequestAndDecodeResponse) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce(RunOnceCallback<0>(OAuthTokenGetter::Status::SUCCESS, "",
                                   kFakeAccessToken));

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback,
              Run(HasStatusCode(net::HTTP_OK), IsResponseText()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  // Verify request.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  std::string auth_header;
  ASSERT_TRUE(pending_request->request.headers.GetHeader(
      kAuthorizationHeaderKey, &auth_header));
  ASSERT_EQ(kFakeAccessTokenHeaderValue, auth_header);
  auto& data_element =
      pending_request->request.request_body->elements()->front();
  std::string request_body_data =
      std::string(data_element.bytes(), data_element.length());
  EchoRequest request_message;
  ASSERT_TRUE(request_message.ParseFromString(request_body_data));
  ASSERT_EQ(kRequestText, request_message.text());

  // Respond.
  test_url_loader_factory_.AddResponse(kTestFullUrl,
                                       CreateDefaultResponseContent());
  run_loop.Run();
}

TEST_F(ProtobufHttpClientTest,
       SendUnauthenticatedRequest_TokenGetterNotCalled) {
  EXPECT_CALL(mock_token_getter_, CallWithToken(_)).Times(0);

  auto request = CreateDefaultTestRequest();
  request->authenticated = false;
  client_.ExecuteRequest(std::move(request));

  // Verify that the request is sent with no auth header.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_FALSE(
      pending_request->request.headers.HasHeader(kAuthorizationHeaderKey));
}

TEST_F(ProtobufHttpClientTest,
       FailedToFetchAuthToken_RejectsWithUnauthorizedError) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce(
          RunOnceCallback<0>(OAuthTokenGetter::Status::AUTH_ERROR, "", ""));

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback,
              Run(HasStatusCode(net::HTTP_UNAUTHORIZED), IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  run_loop.Run();
}

TEST_F(ProtobufHttpClientTest, FailedToParseResponse_GetsInvalidResponseError) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce(RunOnceCallback<0>(OAuthTokenGetter::Status::SUCCESS, "",
                                   kFakeAccessToken));

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(
      response_callback,
      Run(HasNetError(net::Error::ERR_INVALID_RESPONSE), IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  // Respond.
  test_url_loader_factory_.AddResponse(kTestFullUrl, "Invalid content");
  run_loop.Run();
}

TEST_F(ProtobufHttpClientTest, ServerRespondsWithError) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce(RunOnceCallback<0>(OAuthTokenGetter::Status::SUCCESS, "", ""));

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback,
              Run(HasStatusCode(net::HTTP_UNAUTHORIZED), IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  test_url_loader_factory_.AddResponse(kTestFullUrl, "",
                                       net::HttpStatusCode::HTTP_UNAUTHORIZED);
  run_loop.Run();
}

TEST_F(ProtobufHttpClientTest, CancelPendingRequests_CallbackNotCalled) {
  base::RunLoop run_loop;

  OAuthTokenGetter::TokenCallback token_callback;
  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce([&](OAuthTokenGetter::TokenCallback callback) {
        token_callback = std::move(callback);
      });

  auto request = CreateDefaultTestRequest();
  client_.ExecuteRequest(std::move(request));
  client_.CancelPendingRequests();
  ASSERT_TRUE(token_callback);
  std::move(token_callback)
      .Run(OAuthTokenGetter::Status::SUCCESS, "", kFakeAccessToken);

  // Verify no request.
  ASSERT_FALSE(test_url_loader_factory_.IsPending(kTestFullUrl));
}

}  // namespace remoting
