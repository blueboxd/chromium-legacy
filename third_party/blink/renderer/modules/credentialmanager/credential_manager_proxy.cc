// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_proxy.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

CredentialManagerProxy::CredentialManagerProxy(Document& document)
    : document_(document) {
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);
  frame->GetBrowserInterfaceBroker().GetInterface(
      credential_manager_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
  frame->GetBrowserInterfaceBroker().GetInterface(
      authenticator_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
}

CredentialManagerProxy::~CredentialManagerProxy() = default;

mojom::blink::SmsReceiver* CredentialManagerProxy::SmsReceiver() {
  if (!sms_receiver_) {
    LocalFrame* frame = document_->GetFrame();
    DCHECK(frame);
    frame->GetBrowserInterfaceBroker().GetInterface(
        sms_receiver_.BindNewPipeAndPassReceiver(
            frame->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return sms_receiver_.get();
}

// static
CredentialManagerProxy* CredentialManagerProxy::From(Document& document) {
  auto* supplement =
      Supplement<Document>::From<CredentialManagerProxy>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<CredentialManagerProxy>(document);
    ProvideTo(document, supplement);
  }
  return supplement;
}

// static
CredentialManagerProxy* CredentialManagerProxy::From(
    ScriptState* script_state) {
  DCHECK(script_state->ContextIsValid());
  return From(Document::From(*ExecutionContext::From(script_state)));
}

void CredentialManagerProxy::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  Supplement<Document>::Trace(visitor);
}

// static
const char CredentialManagerProxy::kSupplementName[] = "CredentialManagerProxy";

}  // namespace blink
