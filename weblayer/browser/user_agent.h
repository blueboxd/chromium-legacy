// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_USER_AGENT_H_
#define WEBLAYER_BROWSER_USER_AGENT_H_

#include <string>

namespace blink {
struct UserAgentMetadata;
}

namespace weblayer {

// Returns the product used in building the user-agent.
std::string GetProduct();

std::string GetUserAgent();

blink::UserAgentMetadata GetUserAgentMetadata();

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_USER_AGENT_H_
