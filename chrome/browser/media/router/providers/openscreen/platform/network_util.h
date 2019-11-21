// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_NETWORK_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_NETWORK_UTIL_H_

#include <memory>

#include "net/base/address_family.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace net {
class IPAddress;
class IPEndPoint;
}  // namespace net

// Helper namespace for stateless methods that convert between Open Screen
// and Chrome net types.
namespace media_router {
namespace network_util {

const net::IPAddress ToChromeNetAddress(const openscreen::IPAddress& address);
const net::IPEndPoint ToChromeNetEndpoint(
    const openscreen::IPEndpoint& endpoint);
openscreen::IPAddress::Version ToOpenScreenVersion(
    const net::AddressFamily family);
const openscreen::IPEndpoint ToOpenScreenEndpoint(
    const net::IPEndPoint& endpoint);

}  // namespace network_util
}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_NETWORK_UTIL_H_
