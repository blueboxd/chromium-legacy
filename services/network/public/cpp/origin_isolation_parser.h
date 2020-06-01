// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_ISOLATION_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_ISOLATION_PARSER_H_

#include <string>
#include "base/component_export.h"

namespace network {

// Parsing is done following the Origin-Isolation spec draft:
// https://github.com/whatwg/html/pull/5545
//
// See the comment in network::PopulateParsedHeaders for restrictions on this
// function.
COMPONENT_EXPORT(NETWORK_CPP)
bool ParseOriginIsolation(const std::string&);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_ISOLATION_PARSER_H_
