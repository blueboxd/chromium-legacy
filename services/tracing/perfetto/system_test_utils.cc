// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/system_test_utils.h"

#include <cstdio>

#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/service_ipc_host.h"
#include "third_party/perfetto/protos/perfetto/common/commit_data_request.pb.h"

namespace tracing {

MockSystemService::MockSystemService(const std::string& consumer_socket,
                                     const std::string& producer_socket)
    : consumer_(consumer_socket),
      producer_(producer_socket),
      task_runner_(std::make_unique<PerfettoTaskRunner>(
          base::SequencedTaskRunnerHandle::Get())) {
  service_ = perfetto::ServiceIPCHost::CreateInstance(task_runner_.get());
  CHECK(service_);
  unlink(producer_socket.c_str());
  unlink(consumer_socket.c_str());
  bool succeeded = service_->Start(producer_.c_str(), consumer_.c_str());
  CHECK(succeeded);
}

MockSystemService::~MockSystemService() {
  service_.reset();
  remove(producer().c_str());
  remove(consumer().c_str());
}

const std::string& MockSystemService::consumer() const {
  return consumer_;
}

const std::string& MockSystemService::producer() const {
  return producer_;
}

perfetto::TracingService* MockSystemService::GetService() {
  return service_->service();
}

MockAndroidSystemProducer::MockAndroidSystemProducer(
    const std::string& socket,
    bool check_sdk_level,
    uint32_t num_data_sources,
    base::OnceClosure data_source_enabled_callback,
    base::OnceClosure data_source_disabled_callback)
    : AndroidSystemProducer(socket.c_str(),
                            PerfettoTracedProcess::Get()->GetTaskRunner()),
      num_data_sources_expected_(num_data_sources),
      data_source_enabled_callback_(std::move(data_source_enabled_callback)),
      data_source_disabled_callback_(std::move(data_source_disabled_callback)) {
  // We want to set the SystemProducer to this mock, but that 'requires' passing
  // ownership of ourselves to PerfettoTracedProcess. Since someone else manages
  // our deletion we need to be careful in the deconstructor to not double free
  // ourselves (so we must call release once we get back our pointer.
  std::unique_ptr<MockAndroidSystemProducer> client;
  client.reset(this);
  old_producer_ = PerfettoTracedProcess::Get()->SetSystemProducerForTesting(
      std::move(client));
  SetDisallowPreAndroidPieForTesting(check_sdk_level);
  Connect();
}

MockAndroidSystemProducer::~MockAndroidSystemProducer() {
  // See comment in the constructor.
  auto client = PerfettoTracedProcess::Get()->SetSystemProducerForTesting(
      std::move(old_producer_));
  CHECK(client.get() == this);
  client.release();
}

void MockAndroidSystemProducer::StartDataSource(
    perfetto::DataSourceInstanceID id,
    const perfetto::DataSourceConfig& data_source_config) {
  AndroidSystemProducer::StartDataSource(id, data_source_config);
  CHECK_LT(num_data_sources_active_, num_data_sources_expected_);
  if (data_source_enabled_callback_ &&
      ++num_data_sources_active_ == num_data_sources_expected_) {
    std::move(data_source_enabled_callback_).Run();
  }
}

void MockAndroidSystemProducer::StopDataSource(
    perfetto::DataSourceInstanceID id) {
  AndroidSystemProducer::StopDataSource(id);
  CHECK_GT(num_data_sources_active_, 0u);
  if (data_source_disabled_callback_ && --num_data_sources_active_ == 0) {
    std::move(data_source_disabled_callback_).Run();
  }
}

void MockAndroidSystemProducer::CommitData(
    const perfetto::CommitDataRequest& commit,
    CommitDataCallback callback) {
  AndroidSystemProducer::CommitData(commit, callback);
}

void MockAndroidSystemProducer::SetDataSourceEnabledCallback(
    base::OnceClosure data_source_enabled_callback) {
  data_source_enabled_callback_ = std::move(data_source_enabled_callback);
}

void MockAndroidSystemProducer::SetDataSourceDisabledCallback(
    base::OnceClosure data_source_disabled_callback) {
  data_source_disabled_callback_ = std::move(data_source_disabled_callback);
}

}  // namespace tracing
