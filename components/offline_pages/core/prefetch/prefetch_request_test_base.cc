// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_entropy_provider.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "services/network/test/test_utils.h"

namespace offline_pages {

const char PrefetchRequestTestBase::kExperimentValueSetInFieldTrial[] =
    "Test Experiment";

PrefetchRequestTestBase::PrefetchRequestTestBase()
    : task_runner_(new base::TestMockTimeTaskRunner),
      test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {
  message_loop_.SetTaskRunner(task_runner_);
}

PrefetchRequestTestBase::~PrefetchRequestTestBase() {}

void PrefetchRequestTestBase::SetUp() {
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        last_resource_request_ = request;
      }));
}

void PrefetchRequestTestBase::SetUpExperimentOption() {
  std::map<std::string, std::string> params;
  params[kPrefetchingOfflinePagesExperimentsOption] =
      kExperimentValueSetInFieldTrial;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kPrefetchingOfflinePagesFeature, params);
}

void PrefetchRequestTestBase::RespondWithNetError(int net_error) {
  int pending_requests_count = test_url_loader_factory_.NumPending();
  DCHECK(pending_requests_count > 0);
  network::URLLoaderCompletionStatus completion_status(net_error);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingRequest(0)->request.url, completion_status,
      network::ResourceResponseHead(), std::string());
}

void PrefetchRequestTestBase::RespondWithHttpError(
    net::HttpStatusCode http_error) {
  int pending_requests_count = test_url_loader_factory_.NumPending();
  auto resource_response_head = network::CreateResourceResponseHead(http_error);
  DCHECK(pending_requests_count > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingRequest(0)->request.url,
      network::URLLoaderCompletionStatus(net::OK), resource_response_head,
      std::string());
}

void PrefetchRequestTestBase::RespondWithData(const std::string& data) {
  DCHECK(test_url_loader_factory_.pending_requests()->size() > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingRequest(0)->request.url.spec(), data);
}

void PrefetchRequestTestBase::RespondWithHttpErrorAndData(
    net::HttpStatusCode http_error,
    const std::string& data) {
  int pending_requests_count = test_url_loader_factory_.NumPending();
  auto resource_response_head = network::CreateResourceResponseHead(http_error);
  DCHECK(pending_requests_count > 0);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetPendingRequest(0)->request.url,
      network::URLLoaderCompletionStatus(net::OK), resource_response_head,
      data);
}

network::TestURLLoaderFactory::PendingRequest*
PrefetchRequestTestBase::GetPendingRequest(size_t index) {
  return test_url_loader_factory_.GetPendingRequest(index);
}

std::string PrefetchRequestTestBase::GetExperiementHeaderValue(
    network::TestURLLoaderFactory::PendingRequest* pending_request) {
  DCHECK(pending_request);

  net::HttpRequestHeaders headers = pending_request->request.headers;

  std::string experiment_header;
  headers.GetHeader(kPrefetchExperimentHeaderName, &experiment_header);
  return experiment_header;
}

void PrefetchRequestTestBase::RunUntilIdle() {
  task_runner_->RunUntilIdle();
}

void PrefetchRequestTestBase::FastForwardUntilNoTasksRemain() {
  task_runner_->FastForwardUntilNoTasksRemain();
}

}  // namespace offline_pages
