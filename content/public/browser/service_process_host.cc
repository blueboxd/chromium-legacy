// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"

namespace content {

ServiceProcessHost::Options::Options() = default;

ServiceProcessHost::Options::~Options() = default;

ServiceProcessHost::Options::Options(Options&&) = default;

ServiceProcessHost::Options& ServiceProcessHost::Options::WithSandboxType(
    SandboxType type) {
  sandbox_type = type;
  return *this;
}

ServiceProcessHost::Options& ServiceProcessHost::Options::WithDisplayName(
    const base::string16& name) {
  display_name = name;
  return *this;
}

ServiceProcessHost::Options& ServiceProcessHost::Options::WithDisplayName(
    int resource_id) {
  display_name = GetContentClient()->GetLocalizedString(resource_id);
  return *this;
}

ServiceProcessHost::Options& ServiceProcessHost::Options::WithChildFlags(
    int flags) {
  child_flags = flags;
  return *this;
}

ServiceProcessHost::Options ServiceProcessHost::Options::Pass() {
  return std::move(*this);
}

}  // namespace content
