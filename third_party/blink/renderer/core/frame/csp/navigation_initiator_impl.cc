// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/navigation_initiator_impl.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

const char NavigationInitiatorImpl::kSupplementName[] =
    "NavigationInitiatorImpl";

NavigationInitiatorImpl& NavigationInitiatorImpl::From(LocalDOMWindow& window) {
  NavigationInitiatorImpl* initiator =
      Supplement<LocalDOMWindow>::From<NavigationInitiatorImpl>(window);
  if (!initiator) {
    initiator = MakeGarbageCollected<NavigationInitiatorImpl>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, initiator);
  }
  return *initiator;
}

NavigationInitiatorImpl::NavigationInitiatorImpl(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      navigation_initiator_receivers_(this, &window) {}

void NavigationInitiatorImpl::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(navigation_initiator_receivers_);
}

void NavigationInitiatorImpl::SendViolationReport(
    network::mojom::blink::CSPViolationPtr violation) {
  auto source_location = std::make_unique<SourceLocation>(
      violation->source_location->url, violation->source_location->line,
      violation->source_location->column, nullptr);

  GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, violation->console_message));

  GetSupplementable()->GetContentSecurityPolicy()->ReportViolation(
      violation->directive,
      ContentSecurityPolicy::GetDirectiveType(violation->effective_directive),
      violation->console_message, KURL(violation->blocked_url),
      violation->report_endpoints, violation->use_reporting_api,
      violation->header, violation->type,
      ContentSecurityPolicy::ContentSecurityPolicyViolationType::kURLViolation,
      std::move(source_location), nullptr /* LocalFrame */,
      violation->after_redirect ? RedirectStatus::kFollowedRedirect
                                : RedirectStatus::kNoRedirect,
      nullptr /* Element */);
}

void NavigationInitiatorImpl::BindReceiver(
    mojo::PendingReceiver<mojom::blink::NavigationInitiator> receiver) {
  navigation_initiator_receivers_.Add(
      std::move(receiver),
      GetSupplementable()->GetTaskRunner(TaskType::kNetworking));
}

}  // namespace blink
