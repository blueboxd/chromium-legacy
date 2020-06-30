// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "services/network/cross_origin_read_blocking_exception_for_plugin.h"

namespace network {

namespace {

std::set<int>& GetPluginProxyingProcesses() {
  static base::NoDestructor<std::set<int>> set;
  return *set;
}

}  // namespace

// static
void CrossOriginReadBlockingExceptionForPlugin::AddExceptionForPlugin(
    int process_id) {
  std::set<int>& plugin_proxies = GetPluginProxyingProcesses();
  plugin_proxies.insert(process_id);
}

// static
bool CrossOriginReadBlockingExceptionForPlugin::ShouldAllowForPlugin(
    int process_id) {
  std::set<int>& plugin_proxies = GetPluginProxyingProcesses();
  return base::Contains(plugin_proxies, process_id);
}

// static
void CrossOriginReadBlockingExceptionForPlugin::RemoveExceptionForPlugin(
    int process_id) {
  std::set<int>& plugin_proxies = GetPluginProxyingProcesses();
  plugin_proxies.erase(process_id);
}

}  // namespace network
