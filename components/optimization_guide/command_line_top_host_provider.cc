// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/command_line_top_host_provider.h"

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "components/optimization_guide/optimization_guide_switches.h"

namespace optimization_guide {

// static
std::unique_ptr<CommandLineTopHostProvider>
CommandLineTopHostProvider::CreateIfEnabled() {
  base::Optional<std::vector<std::string>> top_hosts =
      switches::ParseHintsFetchOverrideFromCommandLine();
  if (top_hosts) {
    // Note: wrap_unique is used because the constructor is private.
    return base::WrapUnique(new CommandLineTopHostProvider(*top_hosts));
  }

  return nullptr;
}

CommandLineTopHostProvider::CommandLineTopHostProvider(
    const std::vector<std::string>& top_hosts)
    : top_hosts_(top_hosts) {}

CommandLineTopHostProvider::~CommandLineTopHostProvider() = default;

std::vector<std::string> CommandLineTopHostProvider::GetTopHosts(
    size_t max_sites) {
  if (top_hosts_.size() <= max_sites) {
    return top_hosts_;
  }

  std::vector<std::string> top_hosts;
  top_hosts.reserve(max_sites);
  for (const auto& top_host : top_hosts_) {
    if (top_hosts.size() >= max_sites)
      return top_hosts;

    top_hosts.push_back(top_host);
  }
  return top_hosts;
}

}  // namespace optimization_guide
