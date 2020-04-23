// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"

#include <string>

#include "base/base64.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

// Creates a new ClientConfig.
ClientConfig CreateClientConfig(const std::string& session_key,
                                int64_t expire_duration_seconds,
                                int64_t expire_duration_nanoseconds) {
  ClientConfig config;

  config.set_session_key(session_key);
  config.mutable_refresh_duration()->set_seconds(expire_duration_seconds);
  config.mutable_refresh_duration()->set_nanos(expire_duration_nanoseconds);
  config.mutable_proxy_config()->clear_http_proxy_servers();
  return config;
}

// Takes |config| and returns the base64 encoding of its serialized byte stream.
std::string EncodeConfig(const ClientConfig& config) {
  std::string config_data;
  std::string encoded_data;
  EXPECT_TRUE(config.SerializeToString(&config_data));
  base::Base64Encode(config_data, &encoded_data);
  return encoded_data;
}

std::string DummyBase64Config() {
  ClientConfig config = CreateClientConfig("secureSessionKey", 600, 0);
  return EncodeConfig(config);
}

}  // namespace data_reduction_proxy
