// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_client.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/update_client/network.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/updater/win/net/network.h"
#elif defined(OS_MAC)
#include "chrome/updater/mac/net/network.h"
#endif  // OS_WIN

using base::test::RunClosure;

namespace updater {

namespace {

class TestTokenService : public TokenServiceInterface {
 public:
  TestTokenService(const std::string& enrollment_token,
                   const std::string& dm_token)
      : enrollment_token_(enrollment_token), dm_token_(dm_token) {}
  ~TestTokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return "TestDeviceID"; }

  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    enrollment_token_ = enrollment_token;
    return true;
  }

  std::string GetEnrollmentToken() const override { return enrollment_token_; }

  bool StoreDmToken(const std::string& dm_token) override {
    dm_token_ = dm_token;
    return true;
  }
  std::string GetDmToken() const override { return dm_token_; }

 private:
  std::string enrollment_token_;
  std::string dm_token_;
};

class TestConfigurator : public DMClient::Configurator {
 public:
  explicit TestConfigurator(const GURL& url);
  ~TestConfigurator() override = default;

  std::string GetDMServerUrl() const override { return server_url_; }

  std::string GetAgentParameter() const override {
    return "Updater-Test-Agent";
  }

  std::string GetPlatformParameter() const override { return "Test-Platform"; }

  std::unique_ptr<update_client::NetworkFetcher> CreateNetworkFetcher()
      const override {
    return network_fetcher_factory_->Create();
  }

 private:
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  const std::string server_url_;
};

TestConfigurator::TestConfigurator(const GURL& url)
    : network_fetcher_factory_(base::MakeRefCounted<NetworkFetcherFactory>()),
      server_url_(url.spec()) {}

}  // namespace

class DMClientTest : public ::testing::Test {
 public:
  ~DMClientTest() override = default;

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::string app_type;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "apptype", &app_type));
    EXPECT_EQ(app_type, "Chrome");

    std::string platform;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "platform", &platform));
    EXPECT_EQ(platform, "Test-Platform");

    std::string device_id;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "deviceid", &device_id));
    EXPECT_EQ(device_id, "TestDeviceID");

    EXPECT_EQ(request.headers.at("Content-Type"), "application/x-protobuf");

    std::string request_type;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(request.GetURL(), "request", &request_type));
    if (request_type == "register_policy_agent")
      return HandleRegisterRequest(request);

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return http_response;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRegisterRequest(
      const net::test_server::HttpRequest& request) {
    std::string authorization = request.headers.at("Authorization");
    EXPECT_EQ(authorization, "GoogleEnrollmentToken token=TestEnrollmentToken");

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(response_http_status_);
    http_response->set_content_type("application/x-protobuf");
    http_response->set_content(response_body_);
    return http_response;
  }

  void OnRegisterRequestComplete(DMClient::RequestResult result) {
    EXPECT_EQ(result, expected_result_);
    if (result == DMClient::RequestResult::kSuccess ||
        result == DMClient::RequestResult::kAleadyRegistered) {
      EXPECT_EQ(storage_->GetDmToken(), "TestDMToken");
    } else {
      EXPECT_TRUE(storage_->GetDmToken().empty());
    }
    PostRequestCompleted();
  }

  void OnDeregisterRequestComplete(DMClient::RequestResult result) {
    EXPECT_EQ(result, DMClient::RequestResult::kSuccess);
    EXPECT_TRUE(storage_->IsDeviceDeregistered());
    PostRequestCompleted();
  }

  MOCK_METHOD0(PostRequestCompleted, void(void));

  void CreateStorage(const base::FilePath& root_path, bool initializeDMToken) {
    constexpr char kEnrollmentToken[] = "TestEnrollmentToken";
    constexpr char kDmToken[] = "TestDMToken";
    storage_ = base::MakeRefCounted<DMStorage>(
        root_path, std::make_unique<TestTokenService>(
                       kEnrollmentToken, initializeDMToken ? kDmToken : ""));
  }

  void StartTestServerWithResponse(net::HttpStatusCode http_status,
                                   const std::string& body) {
    response_http_status_ = http_status;
    response_body_ = body;
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &DMClientTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  std::string GetDefaultDeviceRegisterResponse() const {
    auto dm_response =
        std::make_unique<enterprise_management::DeviceManagementResponse>();
    dm_response->mutable_register_response()->set_device_management_token(
        "TestDMToken");
    return dm_response->SerializeAsString();
  }

  std::unique_ptr<DMClient> CreateDMClient() const {
    const GURL url = test_server_.GetURL("/dm_api");
    auto test_config = std::make_unique<TestConfigurator>(url);
    return std::make_unique<DMClient>(std::move(test_config), storage_);
  }

  void SetExpectedRequestResult(DMClient::RequestResult expected_result) {
    expected_result_ = expected_result;
  }

  scoped_refptr<DMStorage> storage_;
  net::EmbeddedTestServer test_server_;
  net::HttpStatusCode response_http_status_ = net::HTTP_OK;
  std::string response_body_;

  DMClient::RequestResult expected_result_ = DMClient::RequestResult::kSuccess;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DMClientTest, PostRegisterRequest_Success) {
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  CreateStorage(cache_root.GetPath(), false /* init DM token */);
  StartTestServerWithResponse(net::HTTP_OK, GetDefaultDeviceRegisterResponse());

  SetExpectedRequestResult(DMClient::RequestResult::kSuccess);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, PostRequestCompleted()).WillOnce(RunClosure(quit_closure));

  std::unique_ptr<DMClient> test_client = CreateDMClient();
  test_client->PostRegisterRequest(base::BindOnce(
      &DMClientTest::OnRegisterRequestComplete, base::Unretained(this)));
  run_loop.Run();
}

TEST_F(DMClientTest, PostRegisterRequest_Deregister) {
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  CreateStorage(cache_root.GetPath(), false /* init DM token */);
  StartTestServerWithResponse(net::HTTP_GONE, "" /* response body */);

  SetExpectedRequestResult(DMClient::RequestResult::kSuccess);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, PostRequestCompleted()).WillOnce(RunClosure(quit_closure));

  std::unique_ptr<DMClient> test_client = CreateDMClient();
  test_client->PostRegisterRequest(base::BindOnce(
      &DMClientTest::OnDeregisterRequestComplete, base::Unretained(this)));
  run_loop.Run();
}

TEST_F(DMClientTest, PostRegisterRequest_BadRequest) {
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  CreateStorage(cache_root.GetPath(), false /* init DM token */);
  StartTestServerWithResponse(net::HTTP_BAD_REQUEST, "" /* response body */);

  SetExpectedRequestResult(DMClient::RequestResult::kHttpError);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, PostRequestCompleted()).WillOnce(RunClosure(quit_closure));

  std::unique_ptr<DMClient> test_client = CreateDMClient();
  test_client->PostRegisterRequest(base::BindOnce(
      &DMClientTest::OnRegisterRequestComplete, base::Unretained(this)));
  run_loop.Run();
}

TEST_F(DMClientTest, PostRegisterRequest_AlreadyRegistered) {
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  CreateStorage(cache_root.GetPath(), true /* init DM token */);
  StartTestServerWithResponse(net::HTTP_OK, GetDefaultDeviceRegisterResponse());

  SetExpectedRequestResult(DMClient::RequestResult::kAleadyRegistered);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, PostRequestCompleted()).WillOnce(RunClosure(quit_closure));

  std::unique_ptr<DMClient> test_client = CreateDMClient();
  test_client->PostRegisterRequest(base::BindOnce(
      &DMClientTest::OnRegisterRequestComplete, base::Unretained(this)));
  run_loop.Run();
}

TEST_F(DMClientTest, PostRegisterRequest_BadResponseData) {
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  CreateStorage(cache_root.GetPath(), false /* init DM token */);
  StartTestServerWithResponse(net::HTTP_OK, "BadResponseData");

  SetExpectedRequestResult(DMClient::RequestResult::kUnexpectedResponse);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, PostRequestCompleted()).WillOnce(RunClosure(quit_closure));

  std::unique_ptr<DMClient> test_client = CreateDMClient();
  test_client->PostRegisterRequest(base::BindOnce(
      &DMClientTest::OnRegisterRequestComplete, base::Unretained(this)));
  run_loop.Run();
}

}  // namespace updater
