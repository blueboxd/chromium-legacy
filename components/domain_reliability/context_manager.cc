// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context_manager.h"

#include <utility>

#include "base/bind_helpers.h"
#include "net/base/url_util.h"

namespace domain_reliability {

DomainReliabilityContextManager::DomainReliabilityContextManager(
    DomainReliabilityContext::Factory* context_factory)
    : context_factory_(context_factory) {
}

DomainReliabilityContextManager::~DomainReliabilityContextManager() {
  RemoveContexts(base::NullCallback() /* no filter - delete everything */);
}

void DomainReliabilityContextManager::RouteBeacon(
    std::unique_ptr<DomainReliabilityBeacon> beacon) {
  DomainReliabilityContext* context = GetContextForHost(beacon->url.host());
  if (!context)
    return;

  context->OnBeacon(std::move(beacon));
}

void DomainReliabilityContextManager::ClearBeacons(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  for (auto& context_entry : contexts_) {
    if (origin_filter.is_null() ||
        origin_filter.Run(context_entry.second->config().origin)) {
      context_entry.second->ClearBeacons();
    }
  }
}

DomainReliabilityContext* DomainReliabilityContextManager::AddContextForConfig(
    std::unique_ptr<const DomainReliabilityConfig> config) {
  std::string key = config->origin.host();
  // TODO(juliatuttle): Convert this to actual origin.

  std::unique_ptr<DomainReliabilityContext> context =
      context_factory_->CreateContextForConfig(std::move(config));
  DomainReliabilityContext** entry = &contexts_[key];
  if (*entry)
    delete *entry;

  *entry = context.release();
  return *entry;
}

void DomainReliabilityContextManager::RemoveContexts(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  for (auto it = contexts_.begin(); it != contexts_.end();) {
    if (!origin_filter.is_null() &&
        !origin_filter.Run(it->second->config().origin)) {
      ++it;
      continue;
    }

    delete it->second;
    it = contexts_.erase(it);
  }
}

std::unique_ptr<base::Value> DomainReliabilityContextManager::GetWebUIData()
    const {
  std::unique_ptr<base::ListValue> contexts_value(new base::ListValue());
  for (const auto& context_entry : contexts_)
    contexts_value->Append(context_entry.second->GetWebUIData());
  return std::move(contexts_value);
}

DomainReliabilityContext* DomainReliabilityContextManager::GetContextForHost(
    const std::string& host) {
  ContextMap::const_iterator context_it;

  context_it = contexts_.find(host);
  if (context_it != contexts_.end())
    return context_it->second;

  // TODO(juliatuttle): Make sure parent is not in PSL before using.
  std::string parent_host = net::GetSuperdomain(host);
  if (parent_host.empty())
    return nullptr;

  context_it = contexts_.find(parent_host);
  if (context_it != contexts_.end()
      && context_it->second->config().include_subdomains) {
    return context_it->second;
  }

  return nullptr;
}

}  // namespace domain_reliability
