// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/audits_handler.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {
namespace protocol {

AuditsHandler::AuditsHandler()
    : DevToolsDomainHandler(Audits::Metainfo::domainName) {}
AuditsHandler::~AuditsHandler() = default;

// static
std::vector<AuditsHandler*> AuditsHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<AuditsHandler>(Audits::Metainfo::domainName);
}

void AuditsHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Audits::Frontend>(dispatcher->channel());
  Audits::Dispatcher::wire(dispatcher, this);
}

DispatchResponse AuditsHandler::Disable() {
  enabled_ = false;
  return Response::FallThrough();
}

DispatchResponse AuditsHandler::Enable() {
  enabled_ = true;
  return Response::FallThrough();
}

void AuditsHandler::OnIssueAdded(protocol::Audits::InspectorIssue* issue) {
  if (enabled_) {
    frontend_->IssueAdded(issue->clone());
  }
}

}  // namespace protocol
}  // namespace content
