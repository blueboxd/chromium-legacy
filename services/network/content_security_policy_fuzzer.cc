// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy.h"

#include <string>

#include "net/http/http_response_headers.h"
#include "testing/libfuzzer/libfuzzer_exports.h"

namespace network {

int LLVMFuzzerInitialize(int* argc, char*** argv) {
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ContentSecurityPolicy policy;
  policy.Parse(std::string(reinterpret_cast<const char*>(data), size));

  return 0;
}

}  // namespace network

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  return network::LLVMFuzzerInitialize(argc, argv);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return network::LLVMFuzzerTestOneInput(data, size);
}
