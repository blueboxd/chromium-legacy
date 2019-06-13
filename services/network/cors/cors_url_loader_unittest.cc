// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resource_scheduler.h"
#include "services/network/resource_scheduler_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace cors {

namespace {

const uint32_t kRendererProcessId = 573;

constexpr char kTestCorsExemptHeader[] = "x-test-cors-exempt";

class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  TestURLLoaderFactory() : weak_factory_(this) {}
  ~TestURLLoaderFactory() override = default;

  base::WeakPtr<TestURLLoaderFactory> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void NotifyClientOnReceiveResponse(
      int status_code,
      const std::vector<std::string>& extra_headers) {
    ResourceResponseHead response;
    response.headers = new net::HttpResponseHeaders(
        base::StringPrintf("HTTP/1.1 %d OK\n"
                           "Content-Type: image/png\n",
                           status_code));
    for (const auto& header : extra_headers)
      response.headers->AddHeader(header);

    client_ptr_->OnReceiveResponse(response);
  }

  void NotifyClientOnComplete(int error_code) {
    DCHECK(client_ptr_);
    client_ptr_->OnComplete(URLLoaderCompletionStatus(error_code));
  }

  void NotifyClientOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const std::vector<std::string>& extra_headers) {
    ResourceResponseHead response;
    response.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        base::StringPrintf("HTTP/1.1 %d\n", redirect_info.status_code));
    for (const auto& header : extra_headers)
      response.headers->AddHeader(header);

    client_ptr_->OnReceiveRedirect(redirect_info, response);
  }

  bool IsCreateLoaderAndStartCalled() { return !!client_ptr_; }

  void SetOnCreateLoaderAndStart(const base::RepeatingClosure& closure) {
    on_create_loader_and_start_ = closure;
  }

  const ResourceRequest& request() const { return request_; }
  const GURL& GetRequestedURL() const { return request_.url; }
  int num_created_loaders() const { return num_created_loaders_; }

 private:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    ++num_created_loaders_;
    DCHECK(client);
    request_ = resource_request;
    client_ptr_ = std::move(client);

    if (on_create_loader_and_start_)
      on_create_loader_and_start_.Run();
  }

  void Clone(mojom::URLLoaderFactoryRequest request) override { NOTREACHED(); }

  mojom::URLLoaderClientPtr client_ptr_;

  ResourceRequest request_;

  int num_created_loaders_ = 0;

  base::RepeatingClosure on_create_loader_and_start_;

  base::WeakPtrFactory<TestURLLoaderFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

class CorsURLLoaderTest : public testing::Test {
 public:
  using ReferrerPolicy = net::URLRequest::ReferrerPolicy;

  CorsURLLoaderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        resource_scheduler_(true) {
    net::URLRequestContextBuilder context_builder;
    context_builder.set_proxy_resolution_service(
        net::ProxyResolutionService::CreateDirect());
    url_request_context_ = context_builder.Build();
  }

 protected:
  // testing::Test implementation.
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kOutOfBlinkCors, features::kNetworkService}, {});

    network_service_ = NetworkService::CreateForTesting();

    auto context_params = mojom::NetworkContextParams::New();
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    context_params->cors_exempt_header_list.push_back(kTestCorsExemptHeader);
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(), mojo::MakeRequest(&network_context_ptr_),
        std::move(context_params));

    ResetFactory(base::nullopt, kRendererProcessId);
  }

  void CreateLoaderAndStart(const GURL& origin,
                            const GURL& url,
                            mojom::FetchRequestMode fetch_request_mode) {
    ResourceRequest request;
    request.fetch_request_mode = fetch_request_mode;
    request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
    request.allow_credentials = false;
    request.method = net::HttpRequestHeaders::kGetMethod;
    request.url = url;
    request.request_initiator = url::Origin::Create(origin);
    CreateLoaderAndStart(request);
  }

  void CreateLoaderAndStart(const ResourceRequest& request) {
    cors_url_loader_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader_), 0 /* routing_id */, 0 /* request_id */,
        mojom::kURLLoadOptionNone, request,
        test_cors_loader_client_.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  bool IsNetworkLoaderStarted() {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->IsCreateLoaderAndStartCalled();
  }

  void NotifyLoaderClientOnReceiveResponse(
      const std::vector<std::string>& extra_headers = {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveResponse(200, extra_headers);
  }

  void NotifyLoaderClientOnReceiveResponse(
      int status_code,
      const std::vector<std::string>& extra_headers = {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveResponse(status_code,
                                                            extra_headers);
  }

  void NotifyLoaderClientOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const std::vector<std::string>& extra_headers = {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveRedirect(redirect_info,
                                                            extra_headers);
  }

  void NotifyLoaderClientOnComplete(int error_code) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnComplete(error_code);
  }

  void FollowRedirect(const std::vector<std::string>& removed_headers = {}) {
    DCHECK(url_loader_);
    url_loader_->FollowRedirect(removed_headers, {} /*modified_headers*/,
                                base::nullopt /*new_url*/);
  }

  void AddHostHeaderAndFollowRedirect() {
    DCHECK(url_loader_);
    net::HttpRequestHeaders modified_headers;
    modified_headers.SetHeader(net::HttpRequestHeaders::kHost, "bar.test");
    url_loader_->FollowRedirect({},  // removed_headers
                                modified_headers,
                                base::nullopt);  // new_url
  }

  const ResourceRequest& GetRequest() const {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->request();
  }

  const GURL& GetRequestedURL() {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->GetRequestedURL();
  }

  int num_created_loaders() const {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->num_created_loaders();
  }

  const TestURLLoaderClient& client() const { return test_cors_loader_client_; }
  void ClearHasReceivedRedirect() {
    test_cors_loader_client_.ClearHasReceivedRedirect();
  }

  void RunUntilCreateLoaderAndStartCalled() {
    DCHECK(test_url_loader_factory_);
    base::RunLoop run_loop;
    test_url_loader_factory_->SetOnCreateLoaderAndStart(run_loop.QuitClosure());
    run_loop.Run();
    test_url_loader_factory_->SetOnCreateLoaderAndStart({});
  }
  void RunUntilComplete() { test_cors_loader_client_.RunUntilComplete(); }
  void RunUntilRedirectReceived() {
    test_cors_loader_client_.RunUntilRedirectReceived();
  }

  void AddAllowListEntryForOrigin(const url::Origin& source_origin,
                                  const std::string& protocol,
                                  const std::string& domain,
                                  const mojom::CorsDomainMatchMode mode) {
    origin_access_list_.AddAllowListEntryForOrigin(
        source_origin, protocol, domain, /*port=*/0, mode,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  }

  void AddBlockListEntryForOrigin(const url::Origin& source_origin,
                                  const std::string& protocol,
                                  const std::string& domain,
                                  const mojom::CorsDomainMatchMode mode) {
    origin_access_list_.AddBlockListEntryForOrigin(
        source_origin, protocol, domain, /*port=*/0, mode,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kHighPriority);
  }

  void AddFactoryBoundAllowListEntryForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      const mojom::CorsDomainMatchMode mode) {
    factory_bound_allow_patterns_.push_back(mojom::CorsOriginPattern::New(
        protocol, domain, /*port=*/0, mode,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority));
    ResetFactory(source_origin, kRendererProcessId);
  }

  static net::RedirectInfo CreateRedirectInfo(
      int status_code,
      base::StringPiece method,
      const GURL& url,
      base::StringPiece referrer = base::StringPiece(),
      ReferrerPolicy referrer_policy = net::URLRequest::NO_REFERRER) {
    net::RedirectInfo redirect_info;
    redirect_info.status_code = status_code;
    redirect_info.new_method = method.as_string();
    redirect_info.new_url = url;
    redirect_info.new_referrer = referrer.as_string();
    redirect_info.new_referrer_policy = referrer_policy;
    return redirect_info;
  }

  void ResetFactory(base::Optional<url::Origin> initiator,
                    uint32_t process_id) {
    std::unique_ptr<TestURLLoaderFactory> factory =
        std::make_unique<TestURLLoaderFactory>();
    test_url_loader_factory_ = factory->GetWeakPtr();

    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    if (initiator) {
      factory_params->request_initiator_site_lock = *initiator;
      std::vector<network::mojom::CorsOriginPatternPtr> cloned_patterns;
      for (const auto& item : factory_bound_allow_patterns_)
        cloned_patterns.push_back(item.Clone());
      factory_params->factory_bound_allow_patterns = std::move(cloned_patterns);
    }
    factory_params->process_id = process_id;
    factory_params->is_corb_enabled = (process_id != mojom::kBrowserProcessId);
    constexpr int kRouteId = 765;
    auto resource_scheduler_client =
        base::MakeRefCounted<ResourceSchedulerClient>(
            process_id, kRouteId, &resource_scheduler_,
            url_request_context_->network_quality_estimator());
    cors_url_loader_factory_ = std::make_unique<CorsURLLoaderFactory>(
        network_context_.get(), std::move(factory_params),
        resource_scheduler_client,
        mojo::MakeRequest(&cors_url_loader_factory_ptr_), &origin_access_list_,
        std::move(factory));
  }

 private:
  // Testing instance to enable kOutOfBlinkCors feature.
  base::test::ScopedFeatureList feature_list_;

  // Test environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  ResourceScheduler resource_scheduler_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojom::NetworkContextPtr network_context_ptr_;

  // CorsURLLoaderFactory instance under tests.
  std::unique_ptr<mojom::URLLoaderFactory> cors_url_loader_factory_;
  mojom::URLLoaderFactoryPtr cors_url_loader_factory_ptr_;

  // Factory bound origin access list for testing.
  std::vector<mojom::CorsOriginPatternPtr> factory_bound_allow_patterns_;

  // TestURLLoaderFactory instance owned by CorsURLLoaderFactory.
  base::WeakPtr<TestURLLoaderFactory> test_url_loader_factory_;

  // Holds URLLoaderPtr that CreateLoaderAndStart() creates.
  mojom::URLLoaderPtr url_loader_;

  // TestURLLoaderClient that records callback activities.
  TestURLLoaderClient test_cors_loader_client_;

  // Holds for allowed origin access lists.
  OriginAccessList origin_access_list_;

  DISALLOW_COPY_AND_ASSIGN(CorsURLLoaderTest);
};

class BadMessageTestHelper {
 public:
  BadMessageTestHelper()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {
    mojo::core::SetDefaultProcessErrorCallback(base::BindRepeating(
        &BadMessageTestHelper::OnBadMessage, base::Unretained(this)));
  }

  ~BadMessageTestHelper() {
    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
  }

  const std::vector<std::string>& bad_message_reports() const {
    return bad_message_reports_;
  }

 private:
  void OnBadMessage(const std::string& reason) {
    bad_message_reports_.push_back(reason);
  }

  std::vector<std::string> bad_message_reports_;

  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;

  DISALLOW_COPY_AND_ASSIGN(BadMessageTestHelper);
};

TEST_F(CorsURLLoaderTest, SameOriginWithoutInitiator) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kSameOrigin;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ::testing::ElementsAre("CorsURLLoaderFactory: cors without initiator"));
}

TEST_F(CorsURLLoaderTest, NoCorsWithoutInitiator) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNoCors;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CorsWithoutInitiator) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ::testing::ElementsAre("CorsURLLoaderFactory: cors without initiator"));
}

TEST_F(CorsURLLoaderTest, NavigateWithoutInitiator) {
  ResetFactory(base::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CredentialsModeAndLoadFlagsContradictEachOther1) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ::testing::ElementsAre(
                  "CorsURLLoaderFactory: omit-credentials vs load_flags"));
}

TEST_F(CorsURLLoaderTest, CredentialsModeAndLoadFlagsContradictEachOther2) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ::testing::ElementsAre(
                  "CorsURLLoaderFactory: omit-credentials vs load_flags"));
}

TEST_F(CorsURLLoaderTest, CredentialsModeAndLoadFlagsContradictEachOther3) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ::testing::ElementsAre(
                  "CorsURLLoaderFactory: omit-credentials vs load_flags"));
}

TEST_F(CorsURLLoaderTest, NavigationFromRenderer) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ::testing::ElementsAre(
                  "CorsURLLoaderFactory: navigate from non-browser-process"));
}

TEST_F(CorsURLLoaderTest, SameOriginRequest) {
  const GURL url("http://example.com/foo.png");
  CreateLoaderAndStart(url.GetOrigin(), url,
                       mojom::FetchRequestMode::kSameOrigin);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithNoCorsMode) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kNoCors);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_FALSE(
      GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithNoCorsModeAndPatchMethod) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNoCors;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.method = "PATCH";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  std::string origin_header;
  EXPECT_TRUE(GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin,
                                             &origin_header));
  EXPECT_EQ(origin_header, "http://example.com");
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestFetchRequestModeSameOrigin) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kSameOrigin);

  RunUntilComplete();

  // This call never hits the network URLLoader (i.e. the TestURLLoaderFactory)
  // because it is fails right away.
  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kDisallowedByMode,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithCorsModeButMissingCorsHeader) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  std::string origin_header;
  EXPECT_TRUE(GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin,
                                             &origin_header));
  EXPECT_EQ(origin_header, "http://example.com");
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kMissingAllowOriginHeader,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithCorsMode) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: http://example.com"});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest,
       CrossOriginRequestFetchRequestWithCorsModeButMismatchedCorsHeader) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: http://some-other-domain.com"});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kAllowOriginMismatch,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, StripUsernameAndPassword) {
  const GURL origin("http://example.com");
  const GURL url("http://foo:bar@other.com/foo.png");
  std::string stripped_url = "http://other.com/foo.png";
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: http://example.com"});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_EQ(stripped_url, GetRequestedURL().spec());
}

TEST_F(CorsURLLoaderTest, CorsCheckPassOnRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());
}

TEST_F(CorsURLLoaderTest, CorsCheckFailOnRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(client().completion_status().error_code, net::ERR_FAILED);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(client().completion_status().cors_error_status->cors_error,
            mojom::CorsError::kMissingAllowOriginHeader);
}

TEST_F(CorsURLLoaderTest, NetworkLoaderErrorDuringRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  // Underlying network::URLLoader may call OnComplete with an error at anytime.
  NotifyLoaderClientOnComplete(net::ERR_FAILED);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());
}

TEST_F(CorsURLLoaderTest, SameOriginToSameOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SameOriginToCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://other.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  RunUntilCreateLoaderAndStartCalled();

  // A new loader is created.
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginToCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com"});

  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginToOriginalOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse();

  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  // We got redirected back to the original origin, but we need an
  // access-control-allow-origin header, and we don't have it in this test case.
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(client().completion_status().cors_error_status->cors_error,
            mojom::CorsError::kMissingAllowOriginHeader);
}

TEST_F(CorsURLLoaderTest, CrossOriginToAnotherCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  // The request is tained, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse({"Access-Control-Allow-Origin: null"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest,
       CrossOriginToAnotherCrossOriginRedirectWithPreflight) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  original_request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  original_request.allow_credentials = false;
  original_request.method = "PATCH";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com",
       "Access-Control-Allow-Methods: PATCH"});
  RunUntilCreateLoaderAndStartCalled();

  // the actual request
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "PATCH");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "PATCH", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  // the second preflight request
  EXPECT_EQ(3, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");
  ASSERT_TRUE(GetRequest().request_initiator);
  EXPECT_EQ(GetRequest().request_initiator->Serialize(), "https://example.com");

  // The request is tainted, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse({"Access-Control-Allow-Origin: null",
                                       "Access-Control-Allow-Methods: PATCH"});
  RunUntilCreateLoaderAndStartCalled();

  // the second actual request
  EXPECT_EQ(4, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().method, "PATCH");
  ASSERT_TRUE(GetRequest().request_initiator);
  EXPECT_EQ(GetRequest().request_initiator->Serialize(), "https://example.com");

  // The request is tainted, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse({"Access-Control-Allow-Origin: null"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, RedirectInfoShouldBeUsed) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://other.example.com/foo.png");

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.allow_credentials = false;
  request.method = "POST";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.referrer = url;
  request.referrer_policy =
      net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
  CreateLoaderAndStart(request);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "POST");
  EXPECT_EQ(GetRequest().referrer, url);

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(
      303, "GET", new_url, "https://other.example.com",
      net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().referrer, GURL("https://other.example.com"));
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, TooManyRedirects) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);
  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(1, num_created_loaders());

    GURL new_url(base::StringPrintf("https://example.com/foo.png?%d", i));
    NotifyLoaderClientOnReceiveRedirect(
        CreateRedirectInfo(301, "GET", new_url));

    RunUntilRedirectReceived();
    ASSERT_TRUE(client().has_received_redirect());
    ASSERT_FALSE(client().has_received_response());
    ASSERT_FALSE(client().has_received_completion());

    ClearHasReceivedRedirect();
    FollowRedirect();
  }

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://example.com/bar.png")));
  RunUntilComplete();
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_TOO_MANY_REDIRECTS,
            client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, FollowErrorRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  ResourceRequest original_request;
  original_request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  original_request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  original_request.allow_credentials = false;
  original_request.fetch_redirect_mode = mojom::FetchRedirectMode::kError;
  original_request.method = "GET";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CorsExemptHeaderRemovalOnCrossOriginRedirects) {
  ResourceRequest request;
  request.url = GURL("https://example.com/foo.png");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  request.cors_exempt_headers.SetHeader(kTestCorsExemptHeader, "test-value");
  CreateLoaderAndStart(request);
  EXPECT_EQ(1, num_created_loaders());

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://google.com/bar.png")));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  ASSERT_TRUE(client().has_received_redirect());
  ASSERT_FALSE(client().has_received_response());
  ASSERT_FALSE(client().has_received_completion());
  EXPECT_TRUE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));

  FollowRedirect({kTestCorsExemptHeader});
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_FALSE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));
}

// Tests if OriginAccessList is actually used to decide the cors flag.
// Details for the OriginAccessList behaviors are verified in
// OriginAccessListTest, but this test intends to verify if CorsURlLoader calls
// the list properly.
TEST_F(CorsURLLoaderTest, OriginAccessList_Allowed) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");

  // Adds an entry to allow the cross origin request beyond the CORS
  // rules.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head().response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Check if higher-priority block list wins.
TEST_F(CorsURLLoaderTest, OriginAccessList_Blocked) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");

  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);
  AddBlockListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveResponse();

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

// CorsURLLoader manages two lists, per-NetworkContext list and
// per-URLLoaderFactory list. This test verifies if per-URLLoaderFactory list
// works.
TEST_F(CorsURLLoaderTest, OriginAccessList_AllowedByFactoryList) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");

  AddFactoryBoundAllowListEntryForOrigin(
      url::Origin::Create(origin), url.scheme(), url.host(),
      mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head().response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Checks if CorsURLLoader can respect the per-NetworkContext block list.
TEST_F(CorsURLLoaderTest, OriginAccessList_AllowedByFactoryListButBlocked) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");

  AddFactoryBoundAllowListEntryForOrigin(
      url::Origin::Create(origin), url.scheme(), url.host(),
      mojom::CorsDomainMatchMode::kDisallowSubdomains);
  AddBlockListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveResponse();

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

// Tests if OriginAccessList is actually used to decide response tainting.
TEST_F(CorsURLLoaderTest, OriginAccessList_NoCors) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");

  // Adds an entry to allow the cross origin request without using
  // CORS.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kNoCors);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head().response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, 304ForSimpleRevalidation) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.allow_credentials = false;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.headers.SetHeader("If-Modified-Since", "x");
  request.headers.SetHeader("If-None-Match", "y");
  request.headers.SetHeader("Cache-Control", "z");
  request.is_revalidating = true;
  CreateLoaderAndStart(request);

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(304, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, 304ForSimpleGet) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.allow_credentials = false;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(304, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, 200ForSimpleRevalidation) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.allow_credentials = false;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.headers.SetHeader("If-Modified-Since", "x");
  request.headers.SetHeader("If-None-Match", "y");
  request.headers.SetHeader("Cache-Control", "z");
  request.is_revalidating = true;
  CreateLoaderAndStart(request);

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(200, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, RevalidationAndPreflight) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  original_request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  original_request.allow_credentials = false;
  original_request.method = "GET";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  original_request.headers.SetHeader("If-Modified-Since", "x");
  original_request.headers.SetHeader("If-None-Match", "y");
  original_request.headers.SetHeader("Cache-Control", "z");
  original_request.headers.SetHeader("foo", "bar");
  original_request.is_revalidating = true;
  CreateLoaderAndStart(original_request);

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");
  std::string preflight_request_headers;
  EXPECT_TRUE(GetRequest().headers.GetHeader("access-control-request-headers",
                                             &preflight_request_headers));
  EXPECT_EQ(preflight_request_headers, "foo");

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com",
       "Access-Control-Allow-Headers: foo"});
  RunUntilCreateLoaderAndStartCalled();

  // the actual request
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Keep this in sync with the CalculateResponseTainting test in
// Blink's cors_test.cc.
TEST(CorsURLLoaderTaintingTest, CalculateResponseTainting) {
  using mojom::FetchRequestMode;
  using mojom::FetchResponseType;

  const GURL same_origin_url("https://example.com/");
  const GURL cross_origin_url("https://example2.com/");
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  const base::Optional<url::Origin> no_origin;

  OriginAccessList origin_access_list;

  // CORS flag is false, same-origin request
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kSameOrigin, origin, false,
                false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNoCors, origin, false,
                false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kCors, origin, false, false,
                &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kCorsWithForcedPreflight,
                origin, false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNavigate, origin, false,
                false, &origin_access_list));

  // CORS flag is false, cross-origin request
  EXPECT_EQ(FetchResponseType::kOpaque,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kNoCors, origin, false,
                false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kNavigate, origin, false,
                false, &origin_access_list));

  // CORS flag is true, same-origin request
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kCors, origin, true, false,
                &origin_access_list));
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kCorsWithForcedPreflight,
                origin, true, false, &origin_access_list));

  // CORS flag is true, cross-origin request
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kCors, origin, true, false,
                &origin_access_list));
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kCorsWithForcedPreflight,
                origin, true, false, &origin_access_list));

  // Origin is not provided.
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNoCors, no_origin, false,
                false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNavigate, no_origin, false,
                false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kNoCors, no_origin, false,
                false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kNavigate, no_origin, false,
                false, &origin_access_list));

  // Tainted origin.
  EXPECT_EQ(FetchResponseType::kOpaque,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNoCors, origin, false, true,
                &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kCorsWithForcedPreflight,
                origin, false, true, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNavigate, origin, false,
                true, &origin_access_list));
}

TEST_F(CorsURLLoaderTest, RequestWithHostHeaderFails) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCors;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.allow_credentials = false;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("https://foo.test/path");
  request.request_initiator = url::Origin::Create(GURL("https://foo.test"));
  request.headers.SetHeader(net::HttpRequestHeaders::kHost, "bar.test");
  CreateLoaderAndStart(request);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SetHostHeaderOnRedirectFails) {
  CreateLoaderAndStart(GURL("http://foo.test/"), GURL("http://foo.test/path"),
                       mojom::FetchRequestMode::kCors);

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://redirect.test/")));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());

  ClearHasReceivedRedirect();
  // This should cause the request to fail.
  AddHostHeaderAndFollowRedirect();

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

}  // namespace

}  // namespace cors

}  // namespace network
