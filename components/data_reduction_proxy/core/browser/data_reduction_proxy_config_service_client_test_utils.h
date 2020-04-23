// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_TEST_UTILS_H_

#include <stdint.h>
#include <string>

#include "components/data_reduction_proxy/proto/client_config.pb.h"

namespace data_reduction_proxy {

// Creates a new ClientConfig.
ClientConfig CreateClientConfig(const std::string& session_key,
                                int64_t expire_duration_seconds,
                                int64_t expire_duration_nanoseconds);

// Takes |config| and returns the base64 encoding of its serialized byte stream.
std::string EncodeConfig(const ClientConfig& config);

// Returns a valid ClientConfig in base64 full of dummy values.
std::string DummyBase64Config();

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_TEST_UTILS_H_
