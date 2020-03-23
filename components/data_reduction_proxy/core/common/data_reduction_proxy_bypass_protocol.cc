// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"

#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_server.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

void RecordDataReductionProxyBypassInfo(
    bool is_primary,
    bool bypass_all,
    DataReductionProxyBypassType bypass_type) {
  if (bypass_all) {
    if (is_primary) {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BlockTypePrimary",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BlockTypeFallback",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    }
  } else {
    if (is_primary) {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BypassTypePrimary",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BypassTypeFallback",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    }
  }
}

DataReductionProxyBypassProtocol::DataReductionProxyBypassProtocol() = default;

bool IsProxyBypassedAtTime(const net::ProxyRetryInfoMap& retry_map,
                           const net::ProxyServer& proxy_server,
                           base::TimeTicks t,
                           base::TimeDelta* retry_delay) {
  auto found = retry_map.find(proxy_server.ToURI());

  if (found == retry_map.end() || found->second.bad_until < t) {
    return false;
  }

  if (retry_delay)
    *retry_delay = found->second.current_delay;

  return true;
}

}  // namespace data_reduction_proxy
