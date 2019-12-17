// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_PARSER_H_

#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"

#include <string>

namespace network {

// Parsing is done following the COOP spec draft:
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// TODO(ahemery): add a fuzzer for the parser, see
// services/network/content_security_policy_fuzzer.cc for an example.
mojom::CrossOriginOpenerPolicy COMPONENT_EXPORT(NETWORK_CPP)
    ParseCrossOriginOpenerPolicyHeader(const std::string& raw_coop_string);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_PARSER_H_
