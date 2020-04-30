// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_HTML_PORTAL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_HTML_PORTAL_ELEMENT_H_

#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Document;
class PortalActivateOptions;
class PortalContents;
class ScriptState;

// The HTMLPortalElement implements the <portal> HTML element. The portal
// element can be used to embed another top-level browsing context, which can be
// activated using script. The portal element is still under development and not
// part of the HTML standard. It can be enabled by passing
// --enable-features=Portals. See also https://github.com/WICG/portals.
class CORE_EXPORT HTMLPortalElement : public HTMLFrameOwnerElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLPortalElement(
      Document& document,
      const base::UnguessableToken& portal_token = base::UnguessableToken(),
      mojo::PendingAssociatedRemote<mojom::blink::Portal> remote_portal = {},
      mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
          portal_client_receiver = {});
  ~HTMLPortalElement() override;

  bool IsHTMLPortalElement() const final { return true; }

  // ScriptWrappable overrides.
  void Trace(Visitor* visitor) override;

  // idl implementation.
  ScriptPromise activate(ScriptState*, PortalActivateOptions*, ExceptionState&);
  void postMessage(ScriptState* script_state,
                   const ScriptValue& message,
                   const String& target_origin,
                   const HeapVector<ScriptValue>& transfer,
                   ExceptionState& exception_state);
  void postMessage(ScriptState* script_state,
                   const ScriptValue& message,
                   const WindowPostMessageOptions* options,
                   ExceptionState& exception_state);
  EventListener* onmessage();
  void setOnmessage(EventListener* listener);
  EventListener* onmessageerror();
  void setOnmessageerror(EventListener* listener);

  const base::UnguessableToken& GetToken() const;

  mojom::blink::FrameOwnerElementType OwnerType() const override {
    return mojom::blink::FrameOwnerElementType::kPortal;
  }

  // Consumes the portal interface. When a Portal is activated, or if the
  // renderer receives a connection error, this function will gracefully
  // terminate the portal interface.
  void ConsumePortal();

  // Invoked when this element should no longer keep its guest contents alive
  // due to recent adoption.
  void ExpireAdoptionLifetime();

  // Called by PortalContents when it is about to be destroyed.
  void PortalContentsWillBeDestroyed(PortalContents*);

 private:
  // Checks whether the Portals feature is enabled for this document, and logs a
  // warning to the developer if not. Doing basically anything with an
  // HTMLPortalElement in a document which doesn't support portals is forbidden.
  bool CheckPortalsEnabledOrWarn() const;
  bool CheckPortalsEnabledOrThrow(ExceptionState&) const;

  enum class GuestContentsEligibility {
    // Can have a guest contents.
    kEligible,

    // Ineligible as it is not top-level.
    kNotTopLevel,

    // Ineligible as the host's protocol is not in the HTTP family.
    kNotHTTPFamily,

    // Ineligible for additional reasons.
    kIneligible,
  };
  GuestContentsEligibility GetGuestContentsEligibility() const;
  bool CanHaveGuestContents() const {
    return GetGuestContentsEligibility() == GuestContentsEligibility::kEligible;
  }

  // Navigates the portal to |url_|.
  void Navigate();

  // Node overrides
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void DefaultEventHandler(Event&) override;

  // Element overrides
  bool IsURLAttribute(const Attribute&) const override;
  void ParseAttribute(const AttributeModificationParams&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  bool SupportsFocus() const override;

  // HTMLFrameOwnerElement overrides
  void DisconnectContentFrame() override;
  ParsedFeaturePolicy ConstructContainerPolicy(Vector<String>*) const override {
    return ParsedFeaturePolicy();
  }
  void AttachLayoutTree(AttachContext& context) override;
  network::mojom::ReferrerPolicy ReferrerPolicyAttribute() override;

  Member<PortalContents> portal_;

  network::mojom::ReferrerPolicy referrer_policy_ =
      network::mojom::ReferrerPolicy::kDefault;

  // Temporarily set to keep this element alive after adoption.
  bool was_just_adopted_ = false;
};

// Type casting. Custom since adoption could lead to an HTMLPortalElement ending
// up in a document that doesn't have Portals enabled.
template <>
struct DowncastTraits<HTMLPortalElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLPortalElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node))
      return html_element->IsHTMLPortalElement();
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_HTML_PORTAL_ELEMENT_H_
