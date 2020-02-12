// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_

#include "net/http/structured_headers.h"

namespace blink {
namespace http_structured_header {

using net::structured_headers::Item;
using net::structured_headers::List;
using net::structured_headers::ListOfLists;
using net::structured_headers::ParameterisedIdentifier;
using net::structured_headers::ParameterisedList;
using net::structured_headers::ParameterizedMember;

using net::structured_headers::ParseItem;
using net::structured_headers::ParseList;
using net::structured_headers::ParseListOfLists;
using net::structured_headers::ParseParameterisedList;
using net::structured_headers::SerializeItem;
using net::structured_headers::SerializeList;

}  // namespace http_structured_header
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_
