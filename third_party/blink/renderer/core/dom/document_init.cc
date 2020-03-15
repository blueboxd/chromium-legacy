/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/dom/document_init.h"

#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

// FIXME: Broken with OOPI.
static Document* ParentDocument(DocumentLoader* loader) {
  DCHECK(loader);
  DCHECK(loader->GetFrame());

  Element* owner_element = loader->GetFrame()->DeprecatedLocalOwner();
  if (!owner_element)
    return nullptr;
  return &owner_element->GetDocument();
}

DocumentInit DocumentInit::Create() {
  return DocumentInit(nullptr);
}

DocumentInit DocumentInit::CreateWithImportsController(
    HTMLImportsController* controller) {
  DCHECK(controller);
  Document* master = controller->Master();
  return DocumentInit(controller)
      .WithContextDocument(master->ContextDocument())
      .WithRegistrationContext(master->RegistrationContext());
}

DocumentInit::DocumentInit(HTMLImportsController* imports_controller)
    : imports_controller_(imports_controller),
      create_new_registration_context_(false),
      content_security_policy_from_context_doc_(false) {}

DocumentInit::DocumentInit(const DocumentInit&) = default;

DocumentInit::~DocumentInit() = default;

bool DocumentInit::ShouldSetURL() const {
  DocumentLoader* loader = MasterDocumentLoader();
  return (loader && loader->GetFrame()->Tree().Parent()) || !url_.IsEmpty();
}

bool DocumentInit::IsSrcdocDocument() const {
  // TODO(dgozman): why do we check |parent_document_| here?
  return parent_document_ && is_srcdoc_document_;
}

DocumentLoader* DocumentInit::MasterDocumentLoader() const {
  if (document_loader_)
    return document_loader_;
  if (imports_controller_) {
    return imports_controller_->Master()
        ->GetFrame()
        ->Loader()
        .GetDocumentLoader();
  }
  return nullptr;
}

mojom::blink::WebSandboxFlags DocumentInit::GetSandboxFlags() const {
  mojom::blink::WebSandboxFlags flags = sandbox_flags_;
  if (DocumentLoader* loader = MasterDocumentLoader())
    flags |= loader->GetFrame()->Loader().EffectiveSandboxFlags();
  // If the load was blocked by CSP, force the Document's origin to be unique,
  // so that the blocked document appears to be a normal cross-origin
  // document's load per CSP spec:
  // https://www.w3.org/TR/CSP3/#directive-frame-ancestors.
  if (blocked_by_csp_)
    flags |= mojom::blink::WebSandboxFlags::kOrigin;
  return flags;
}

mojom::blink::InsecureRequestPolicy DocumentInit::GetInsecureRequestPolicy()
    const {
  DCHECK(MasterDocumentLoader());
  Frame* parent_frame = MasterDocumentLoader()->GetFrame()->Tree().Parent();
  if (!parent_frame)
    return mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
  return parent_frame->GetSecurityContext()->GetInsecureRequestPolicy();
}

const SecurityContext::InsecureNavigationsSet*
DocumentInit::InsecureNavigationsToUpgrade() const {
  DCHECK(MasterDocumentLoader());
  Frame* parent_frame = MasterDocumentLoader()->GetFrame()->Tree().Parent();
  if (!parent_frame)
    return nullptr;
  return &parent_frame->GetSecurityContext()->InsecureNavigationsToUpgrade();
}

network::mojom::IPAddressSpace DocumentInit::GetIPAddressSpace() const {
  return ip_address_space_;
}

Settings* DocumentInit::GetSettings() const {
  DCHECK(MasterDocumentLoader());
  return MasterDocumentLoader()->GetFrame()->GetSettings();
}

DocumentInit& DocumentInit::WithDocumentLoader(DocumentLoader* loader) {
  DCHECK(!document_loader_);
  DCHECK(!imports_controller_);
  document_loader_ = loader;
  if (document_loader_)
    parent_document_ = ParentDocument(document_loader_);
  return *this;
}

LocalFrame* DocumentInit::GetFrame() const {
  return document_loader_ ? document_loader_->GetFrame() : nullptr;
}

UseCounter* DocumentInit::GetUseCounter() const {
  return document_loader_;
}

DocumentInit& DocumentInit::WithTypeFrom(const String& type) {
  mime_type_ = type;

  if (GetFrame() && GetFrame()->InViewSourceMode()) {
    type_ = Type::kViewSource;
    return *this;
  }

  // Plugins cannot take HTML and XHTML from us, and we don't even need to
  // initialize the plugin database for those.
  if (type == "text/html") {
    type_ = Type::kHTML;
    return *this;
  }
  if (type == "application/xhtml+xml") {
    type_ = Type::kXHTML;
    return *this;
  }
  // multipart/x-mixed-replace is only supported for images.
  if (MIMETypeRegistry::IsSupportedImageResourceMIMEType(type) ||
      type == "multipart/x-mixed-replace") {
    type_ = Type::kImage;
    return *this;
  }
  if (HTMLMediaElement::GetSupportsType(ContentType(type))) {
    type_ = Type::kMedia;
    return *this;
  }

  PluginData* plugin_data = nullptr;
  if (GetFrame() && GetFrame()->GetPage() &&
      GetFrame()->Loader().AllowPlugins(kNotAboutToInstantiatePlugin)) {
    // If the document is being created for the main frame,
    // frame()->tree().top()->securityContext() returns nullptr.
    // For that reason, the origin must be retrieved directly from url().
    if (GetFrame()->IsMainFrame()) {
      scoped_refptr<const SecurityOrigin> origin =
          SecurityOrigin::Create(Url());
      plugin_data = GetFrame()->GetPage()->GetPluginData(origin.get());
    } else {
      auto* top_security_origin =
          GetFrame()->Tree().Top().GetSecurityContext()->GetSecurityOrigin();
      plugin_data = GetFrame()->GetPage()->GetPluginData(top_security_origin);
    }
  }

  // Everything else except text/plain can be overridden by plugins.
  // Disallowing plugins to use text/plain prevents plugins from hijacking a
  // fundamental type that the browser is expected to handle, and also serves as
  // an optimization to prevent loading the plugin database in the common case.
  if (type != "text/plain" && plugin_data &&
      plugin_data->SupportsMimeType(type)) {
    // Plugins handled by MimeHandlerView do not create a PluginDocument. They
    // are rendered inside cross-process frames and the notion of a PluginView
    // (which is associated with PluginDocument) is irrelevant here.
    if (plugin_data->IsExternalPluginMimeType(type)) {
      type_ = Type::kHTML;
      is_for_external_handler_ = true;
    } else {
      type_ = Type::kPlugin;
      plugin_background_color_ =
          plugin_data->PluginBackgroundColorForMimeType(type);
    }
    return *this;
  }

  if (DOMImplementation::IsTextMIMEType(type))
    type_ = Type::kText;
  else if (type == "image/svg+xml")
    type_ = Type::kSVG;
  else if (DOMImplementation::IsXMLMIMEType(type))
    type_ = Type::kXML;
  else
    type_ = Type::kHTML;
  return *this;
}

DocumentInit& DocumentInit::WithContextDocument(Document* context_document) {
  DCHECK(!context_document_);
  context_document_ = context_document;
  return *this;
}

DocumentInit& DocumentInit::WithURL(const KURL& url) {
  DCHECK(url_.IsNull());
  url_ = url;
  return *this;
}

scoped_refptr<SecurityOrigin> DocumentInit::GetDocumentOrigin() const {
  // Origin to commit is specified by the browser process, it must be taken
  // and used directly. It is currently supplied only for session history
  // navigations, where the origin was already calcuated previously and
  // stored on the session history entry.
  if (origin_to_commit_)
    return origin_to_commit_;

  if (owner_document_)
    return owner_document_->GetMutableSecurityOrigin();

  // Otherwise, create an origin that propagates precursor information
  // as needed. For non-opaque origins, this creates a standard tuple
  // origin, but for opaque origins, it creates an origin with the
  // initiator origin as the precursor.
  return SecurityOrigin::CreateWithReferenceOrigin(url_,
                                                   initiator_origin_.get());
}

DocumentInit& DocumentInit::WithOwnerDocument(Document* owner_document) {
  DCHECK(!owner_document_);
  DCHECK(!initiator_origin_ || !owner_document ||
         owner_document->GetSecurityOrigin() == initiator_origin_);
  owner_document_ = owner_document;
  return *this;
}

DocumentInit& DocumentInit::WithInitiatorOrigin(
    scoped_refptr<const SecurityOrigin> initiator_origin) {
  DCHECK(!initiator_origin_);
  DCHECK(!initiator_origin || !owner_document_ ||
         owner_document_->GetSecurityOrigin() == initiator_origin);
  initiator_origin_ = std::move(initiator_origin);
  return *this;
}

DocumentInit& DocumentInit::WithOriginToCommit(
    scoped_refptr<SecurityOrigin> origin_to_commit) {
  origin_to_commit_ = std::move(origin_to_commit);
  return *this;
}

DocumentInit& DocumentInit::WithIPAddressSpace(
    network::mojom::IPAddressSpace ip_address_space) {
  ip_address_space_ = ip_address_space;
  return *this;
}

DocumentInit& DocumentInit::WithSrcdocDocument(bool is_srcdoc_document) {
  is_srcdoc_document_ = is_srcdoc_document;
  return *this;
}

DocumentInit& DocumentInit::WithBlockedByCSP(bool blocked_by_csp) {
  blocked_by_csp_ = blocked_by_csp;
  return *this;
}

DocumentInit& DocumentInit::WithGrantLoadLocalResources(
    bool grant_load_local_resources) {
  grant_load_local_resources_ = grant_load_local_resources;
  return *this;
}

DocumentInit& DocumentInit::WithRegistrationContext(
    V0CustomElementRegistrationContext* registration_context) {
  DCHECK(!create_new_registration_context_);
  DCHECK(!registration_context_);
  registration_context_ = registration_context;
  return *this;
}

DocumentInit& DocumentInit::WithNewRegistrationContext() {
  DCHECK(!create_new_registration_context_);
  DCHECK(!registration_context_);
  create_new_registration_context_ = true;
  return *this;
}

V0CustomElementRegistrationContext* DocumentInit::RegistrationContext(
    Document* document) const {
  if (!IsA<HTMLDocument>(document) && !document->IsXHTMLDocument())
    return nullptr;

  if (create_new_registration_context_)
    return MakeGarbageCollected<V0CustomElementRegistrationContext>();

  return registration_context_;
}

Document* DocumentInit::ContextDocument() const {
  return context_document_;
}

DocumentInit& DocumentInit::WithFeaturePolicyHeader(const String& header) {
  DCHECK(feature_policy_header_.IsEmpty());
  feature_policy_header_ = header;
  return *this;
}

DocumentInit& DocumentInit::WithReportOnlyFeaturePolicyHeader(
    const String& header) {
  DCHECK(report_only_feature_policy_header_.IsEmpty());
  report_only_feature_policy_header_ = header;
  return *this;
}

DocumentInit& DocumentInit::WithOriginTrialsHeader(const String& header) {
  DCHECK(origin_trials_header_.IsEmpty());
  origin_trials_header_ = header;
  return *this;
}

DocumentInit& DocumentInit::WithSandboxFlags(
    mojom::blink::WebSandboxFlags flags) {
  // Only allow adding more sandbox flags.
  sandbox_flags_ |= flags;
  return *this;
}

DocumentInit& DocumentInit::WithContentSecurityPolicy(
    ContentSecurityPolicy* policy) {
  content_security_policy_ = policy;
  return *this;
}

DocumentInit& DocumentInit::WithContentSecurityPolicyFromContextDoc() {
  content_security_policy_from_context_doc_ = true;
  return *this;
}

ContentSecurityPolicy* DocumentInit::GetContentSecurityPolicy() const {
  DCHECK(
      !(content_security_policy_ && content_security_policy_from_context_doc_));
  if (context_document_ && content_security_policy_from_context_doc_) {
    // Return a copy of the context documents' CSP. The return value will be
    // modified, so this must be a copy.
    ContentSecurityPolicy* csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->CopyStateFrom(context_document_->GetContentSecurityPolicy());
    return csp;
  }
  return content_security_policy_;
}

DocumentInit& DocumentInit::WithFramePolicy(
    const base::Optional<FramePolicy>& frame_policy) {
  frame_policy_ = frame_policy;
  if (frame_policy_.has_value()) {
    DCHECK(document_loader_);
    // Make the snapshot value of sandbox flags from the beginning of navigation
    // available in frame loader, so that the value could be further used to
    // initialize sandbox flags in security context. crbug.com/1026627
    document_loader_->GetFrame()->Loader().SetFrameOwnerSandboxFlags(
        frame_policy_.value().sandbox_flags);
  }
  return *this;
}

DocumentInit& DocumentInit::WithDocumentPolicy(
    const DocumentPolicy::ParsedDocumentPolicy& document_policy) {
  document_policy_ = document_policy;
  return *this;
}

DocumentInit& DocumentInit::WithReportOnlyDocumentPolicyHeader(
    const String& header) {
  DCHECK(report_only_document_policy_header_.IsEmpty());
  report_only_document_policy_header_ = header;
  return *this;
}

DocumentInit& DocumentInit::WithWebBundleClaimedUrl(
    const KURL& web_bundle_claimed_url) {
  web_bundle_claimed_url_ = web_bundle_claimed_url;
  return *this;
}

bool IsPagePopupRunningInWebTest(LocalFrame* frame) {
  return frame && frame->GetPage()->GetChromeClient().IsPopup() &&
         WebTestSupport::IsRunningWebTest();
}

WindowAgentFactory* DocumentInit::GetWindowAgentFactory() const {
  // If we are a page popup in LayoutTests ensure we use the popup
  // owner's frame for looking up the Agent so the tests can possibly
  // access the document via internals API.
  if (IsPagePopupRunningInWebTest(GetFrame())) {
    auto* frame = GetFrame()->PagePopupOwner()->GetDocument().GetFrame();
    return &frame->window_agent_factory();
  }
  if (GetFrame())
    return &GetFrame()->window_agent_factory();
  if (Document* context_document = ContextDocument())
    return context_document->GetWindowAgentFactory();
  if (const Document* owner_document = OwnerDocument())
    return owner_document->GetWindowAgentFactory();
  return nullptr;
}

Settings* DocumentInit::GetSettingsForWindowAgentFactory() const {
  LocalFrame* frame = nullptr;
  if (IsPagePopupRunningInWebTest(GetFrame()))
    frame = GetFrame()->PagePopupOwner()->GetDocument().GetFrame();
  else if (GetFrame())
    frame = GetFrame();
  else if (Document* context_document = ContextDocument())
    frame = context_document->GetFrame();
  else if (const Document* owner_document = OwnerDocument())
    frame = owner_document->GetFrame();
  return frame ? frame->GetSettings() : nullptr;
}

}  // namespace blink
