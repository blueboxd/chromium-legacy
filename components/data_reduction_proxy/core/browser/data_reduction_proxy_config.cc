// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/proxy_server.h"
#include "net/nqe/effective_connection_type.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif  // OS_ANDROID

using base::FieldTrialList;


namespace data_reduction_proxy {

DataReductionProxyConfig::DataReductionProxyConfig()
    : unreachable_(false), enabled_by_user_(false) {
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfig::~DataReductionProxyConfig() {
}

void DataReductionProxyConfig::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& user_agent) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DataReductionProxyConfig::OnNewClientConfigFetched() {
  DCHECK(thread_checker_.CalledOnValidThread());
}


void DataReductionProxyConfig::SetProxyConfig(bool enabled, bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_by_user_ = enabled;
}



void DataReductionProxyConfig::UpdateConfigForTesting(
    bool enabled,
    bool secure_proxies_allowed,
    bool insecure_proxies_allowed) {
  enabled_by_user_ = enabled;
}


bool DataReductionProxyConfig::enabled_by_user_and_reachable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return enabled_by_user_ && !unreachable_;
}

base::TimeTicks DataReductionProxyConfig::GetTicksNow() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::TimeTicks::Now();
}

}  // namespace data_reduction_proxy
