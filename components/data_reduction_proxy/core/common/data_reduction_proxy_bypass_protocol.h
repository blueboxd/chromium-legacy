// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/base/net_errors.h"

namespace data_reduction_proxy {

// Records a data reduction proxy bypass event as a "BlockType" if
// |bypass_all| is true and as a "BypassType" otherwise. Records the event as
// "Primary" if |is_primary| is true and "Fallback" otherwise.
void RecordDataReductionProxyBypassInfo(
    bool is_primary,
    bool bypass_all,
    DataReductionProxyBypassType bypass_type);


// Class responsible for determining when a response should or should not cause
// the data reduction proxy to be bypassed, and to what degree. Owned by the
// DataReductionProxyInterceptor.
class DataReductionProxyBypassProtocol {
 public:
  // Enum values that can be reported for the
  // DataReductionProxy.ResponseProxyServerStatus histogram. These values must
  // be kept in sync with their counterparts in histograms.xml. Visible here for
  // testing purposes.
  enum ResponseProxyServerStatus {
    RESPONSE_PROXY_SERVER_STATUS_EMPTY = 0,
    RESPONSE_PROXY_SERVER_STATUS_DRP,
    RESPONSE_PROXY_SERVER_STATUS_NON_DRP_NO_VIA,
    RESPONSE_PROXY_SERVER_STATUS_NON_DRP_WITH_VIA,
    RESPONSE_PROXY_SERVER_STATUS_MAX
  };

  DataReductionProxyBypassProtocol();


 private:


  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyBypassProtocol);
};

// Returns true if the proxy is on the retry map and the retry delay is not
// expired. If proxy is bypassed, retry_delay (if not NULL) returns the delay
// of proxy_server. If proxy is not bypassed, retry_delay is not assigned.
//
// TODO(http://crbug.com/721403): Move this somewhere better.
bool IsProxyBypassedAtTime(const net::ProxyRetryInfoMap& retry_map,
                           const net::ProxyServer& proxy_server,
                           base::TimeTicks t,
                           base::TimeDelta* retry_delay);


}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_
