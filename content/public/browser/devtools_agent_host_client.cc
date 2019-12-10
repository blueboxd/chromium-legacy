// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/devtools_agent_host_client.h"

namespace content {

bool DevToolsAgentHostClient::MayAttachToURL(const GURL& url, bool is_webui) {
  return true;
}

bool DevToolsAgentHostClient::MayAttachToBrowser() {
  return true;
}

bool DevToolsAgentHostClient::MayReadLocalFiles() {
  return true;
}

bool DevToolsAgentHostClient::MayWriteLocalFiles() {
  return true;
}

bool DevToolsAgentHostClient::UsesBinaryProtocol() {
  return false;
}

}  // namespace content
