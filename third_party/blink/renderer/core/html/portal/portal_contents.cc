// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/portal_contents.h"

#include "third_party/blink/public/mojom/referrer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document_shutdown_observer.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html/portal/portal_post_message_helper.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

PortalContents::PortalContents(
    HTMLPortalElement& portal_element,
    const base::UnguessableToken& portal_token,
    mojo::PendingAssociatedRemote<mojom::blink::Portal> remote_portal,
    mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
        portal_client_receiver)
    : document_(portal_element.GetDocument()),
      portal_element_(&portal_element),
      portal_token_(portal_token),
      remote_portal_(std::move(remote_portal)),
      portal_client_receiver_(this, std::move(portal_client_receiver)) {
  remote_portal_.set_disconnect_handler(
      WTF::Bind(&PortalContents::Destroy, WrapWeakPersistent(this)));
  DocumentPortals::From(GetDocument()).RegisterPortalContents(this);
}

PortalContents::~PortalContents() {}

RemoteFrame* PortalContents::GetFrame() const {
  if (portal_element_)
    return To<RemoteFrame>(portal_element_->ContentFrame());
  return nullptr;
}

ScriptPromise PortalContents::Activate(ScriptState* script_state,
                                       BlinkTransferableMessage data) {
  DCHECK(!IsActivating());
  DCHECK(portal_element_);

  // Mark this contents as having activation in progress.
  DocumentPortals& document_portals = DocumentPortals::From(GetDocument());
  document_portals.SetActivatingPortalContents(this);
  activate_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // Request activation from the browser process.
  // This object (and thus the Mojo connection it owns) remains alive while the
  // renderer awaits the response.
  remote_portal_->Activate(
      std::move(data),
      WTF::Bind(&PortalContents::OnActivateResponse, WrapPersistent(this)));

  // Dissociate from the element. The element is expected to do the same.
  portal_element_ = nullptr;

  return activate_resolver_->Promise();
}

void PortalContents::OnActivateResponse(bool was_adopted) {
  if (was_adopted)
    GetDocument().GetPage()->SetInsidePortal(true);
  activate_resolver_->Resolve();
  activate_resolver_ = nullptr;
  Destroy();
}

void PortalContents::PostMessageToGuest(
    BlinkTransferableMessage message,
    const scoped_refptr<const SecurityOrigin>& target_origin) {
  remote_portal_->PostMessageToGuest(std::move(message), target_origin);
}

void PortalContents::Navigate(
    const KURL& url,
    network::mojom::ReferrerPolicy referrer_policy_to_use) {
  if (url.IsEmpty())
    return;

  if (!url.ProtocolIsInHTTPFamily()) {
    GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kRendering,
        mojom::ConsoleMessageLevel::kWarning,
        "Portals only allow navigation to protocols in the HTTP family."));
    return;
  }

  if (referrer_policy_to_use == network::mojom::ReferrerPolicy::kDefault)
    referrer_policy_to_use = GetDocument().GetReferrerPolicy();
  Referrer referrer = SecurityPolicy::GenerateReferrer(
      referrer_policy_to_use, url, GetDocument().OutgoingReferrer());
  auto mojo_referrer = mojom::blink::Referrer::New(
      KURL(NullURL(), referrer.referrer), referrer.referrer_policy);

  remote_portal_->Navigate(url, std::move(mojo_referrer));
}

void PortalContents::Destroy() {
  DCHECK(!IsActivating());
  if (HTMLPortalElement* element = std::exchange(portal_element_, nullptr))
    element->ConsumePortal();
  portal_token_ = base::UnguessableToken();
  remote_portal_.reset();
  portal_client_receiver_.reset();
  DocumentPortals::From(GetDocument()).DeregisterPortalContents(this);
}

void PortalContents::ForwardMessageFromGuest(
    BlinkTransferableMessage message,
    const scoped_refptr<const SecurityOrigin>& source_origin,
    const scoped_refptr<const SecurityOrigin>& target_origin) {
  if (!IsValid() || !portal_element_)
    return;

  PortalPostMessageHelper::CreateAndDispatchMessageEvent(
      portal_element_, std::move(message), source_origin, target_origin);
}

void PortalContents::DispatchLoadEvent() {
  if (!IsValid() || !portal_element_)
    return;

  portal_element_->DispatchLoad();
  GetDocument().CheckCompleted();
}

void PortalContents::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(portal_element_);
  visitor->Trace(activate_resolver_);
}

}  // namespace blink
