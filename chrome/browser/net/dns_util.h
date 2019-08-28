// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_UTIL_H_
#define CHROME_BROWSER_NET_DNS_UTIL_H_

#include <string>

namespace chrome_browser_net {

// Returns true if the URI template is acceptable for sending requests. If so,
// the |server_method| is set to "GET" if the template contains a "dns" variable
// and to "POST" otherwise. Any "dns" variable may not be part of the hostname,
// and the expanded template must parse to a valid HTTPS URL.
bool IsValidDohTemplate(const std::string& server_template,
                        std::string* server_method);

// Returns true if any machine level policies. ChromeOS devices are already
// handled by the default_for_enterprise_users field on the DoH policy. We don't
// attempt enterprise detection on Android at this time. This special logic is
// to prevent enterprises from having DoH enabled by default and is necessary
// because default_for_enterprise_users only applies to ChromeOS.
bool ShouldDisableDohForManaged();

const char kDnsOverHttpsModeOff[] = "off";
const char kDnsOverHttpsModeAutomatic[] = "automatic";
const char kDnsOverHttpsModeSecure[] = "secure";

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_UTIL_H_
