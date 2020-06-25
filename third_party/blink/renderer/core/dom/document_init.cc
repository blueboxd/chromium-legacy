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

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/sink_document.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_view_source_document.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/media_document.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html/text_document.h"
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

bool IsPagePopupRunningInWebTest(LocalFrame* frame) {
  return frame && frame->GetPage()->GetChromeClient().IsPopup() &&
         WebTestSupport::IsRunningWebTest();
}

// static
DocumentInit DocumentInit::Create() {
  return DocumentInit();
}

DocumentInit::DocumentInit(const DocumentInit&) = default;

DocumentInit::~DocumentInit() = default;

DocumentInit& DocumentInit::ForTest() {
  DCHECK(!execution_context_);
  DCHECK(!document_loader_);
#if DCHECK_IS_ON()
  DCHECK(!for_test_);
  for_test_ = true;
#endif
  content_security_policy_ = MakeGarbageCollected<ContentSecurityPolicy>();
  return *this;
}

DocumentInit& DocumentInit::WithImportsController(
    HTMLImportsController* controller) {
  imports_controller_ = controller;
  return *this;
}

bool DocumentInit::ShouldSetURL() const {
  DocumentLoader* loader = TreeRootDocumentLoader();
  return (loader && loader->GetFrame()->Tree().Parent()) || !url_.IsEmpty();
}

bool DocumentInit::IsSrcdocDocument() const {
  // TODO(dgozman): why do we check |parent_document_| here?
  return parent_document_ && is_srcdoc_document_;
}

DocumentLoader* DocumentInit::TreeRootDocumentLoader() const {
  if (document_loader_)
    return document_loader_;
  if (imports_controller_) {
    return imports_controller_->TreeRoot()
        ->GetFrame()
        ->Loader()
        .GetDocumentLoader();
  }
  return nullptr;
}

network::mojom::blink::WebSandboxFlags DocumentInit::GetSandboxFlags() const {
  DCHECK(content_security_policy_);
  network::mojom::blink::WebSandboxFlags flags =
      sandbox_flags_ | content_security_policy_->GetSandboxMask();
  if (DocumentLoader* loader = TreeRootDocumentLoader())
    flags |= loader->GetFrame()->Loader().EffectiveSandboxFlags();
  return flags;
}

mojom::blink::InsecureRequestPolicy DocumentInit::GetInsecureRequestPolicy()
    const {
  DCHECK(TreeRootDocumentLoader());
  Frame* parent_frame = TreeRootDocumentLoader()->GetFrame()->Tree().Parent();
  if (!parent_frame)
    return mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;
  return parent_frame->GetSecurityContext()->GetInsecureRequestPolicy();
}

const SecurityContext::InsecureNavigationsSet*
DocumentInit::InsecureNavigationsToUpgrade() const {
  DCHECK(TreeRootDocumentLoader());
  Frame* parent_frame = TreeRootDocumentLoader()->GetFrame()->Tree().Parent();
  if (!parent_frame)
    return nullptr;
  return &parent_frame->GetSecurityContext()->InsecureNavigationsToUpgrade();
}

network::mojom::IPAddressSpace DocumentInit::GetIPAddressSpace() const {
  return ip_address_space_;
}

DocumentInit& DocumentInit::WithDocumentLoader(DocumentLoader* loader,
                                               ContentSecurityPolicy* policy) {
  DCHECK(!document_loader_);
  DCHECK(!execution_context_);
  DCHECK(!imports_controller_);
#if DCHECK_IS_ON()
  DCHECK(!for_test_);
#endif
  DCHECK(!content_security_policy_);
  DCHECK(loader);
  DCHECK(policy);
  document_loader_ = loader;
  parent_document_ = ParentDocument(document_loader_);
  content_security_policy_ = policy;
  return *this;
}

LocalFrame* DocumentInit::GetFrame() const {
  return document_loader_ ? document_loader_->GetFrame() : nullptr;
}

UseCounter* DocumentInit::GetUseCounter() const {
  return document_loader_;
}

// static
DocumentInit::Type DocumentInit::ComputeDocumentType(
    LocalFrame* frame,
    const KURL& url,
    const String& mime_type,
    bool* is_for_external_handler) {
  if (frame && frame->InViewSourceMode())
    return Type::kViewSource;

  // Plugins cannot take HTML and XHTML from us, and we don't even need to
  // initialize the plugin database for those.
  if (mime_type == "text/html")
    return Type::kHTML;

  if (mime_type == "application/xhtml+xml")
    return Type::kXHTML;

  // multipart/x-mixed-replace is only supported for images.
  if (MIMETypeRegistry::IsSupportedImageResourceMIMEType(mime_type) ||
      mime_type == "multipart/x-mixed-replace") {
    return Type::kImage;
  }

  if (HTMLMediaElement::GetSupportsType(ContentType(mime_type)))
    return Type::kMedia;

  if (frame && frame->GetPage() &&
      frame->Loader().AllowPlugins(kNotAboutToInstantiatePlugin)) {
    PluginData* plugin_data = GetPluginData(frame, url);

    // Everything else except text/plain can be overridden by plugins.
    // Disallowing plugins to use text/plain prevents plugins from hijacking a
    // fundamental type that the browser is expected to handle, and also serves
    // as an optimization to prevent loading the plugin database in the common
    // case.
    if (mime_type != "text/plain" && plugin_data &&
        plugin_data->SupportsMimeType(mime_type)) {
      // Plugins handled by MimeHandlerView do not create a PluginDocument. They
      // are rendered inside cross-process frames and the notion of a PluginView
      // (which is associated with PluginDocument) is irrelevant here.
      if (plugin_data->IsExternalPluginMimeType(mime_type)) {
        if (is_for_external_handler)
          *is_for_external_handler = true;
        return Type::kHTML;
      }

      return Type::kPlugin;
    }
  }

  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type) ||
      MIMETypeRegistry::IsJSONMimeType(mime_type) ||
      MIMETypeRegistry::IsPlainTextMIMEType(mime_type)) {
    return Type::kText;
  }

  if (mime_type == "image/svg+xml")
    return Type::kSVG;

  if (MIMETypeRegistry::IsXMLMIMEType(mime_type))
    return Type::kXML;

  return Type::kHTML;
}

// static
PluginData* DocumentInit::GetPluginData(LocalFrame* frame, const KURL& url) {
  // If the document is being created for the main frame,
  // frame()->tree().top()->securityContext() returns nullptr.
  // For that reason, the origin must be retrieved directly from |url|.
  if (frame->IsMainFrame())
    return frame->GetPage()->GetPluginData(SecurityOrigin::Create(url).get());

  const SecurityOrigin* main_frame_origin =
      frame->Tree().Top().GetSecurityContext()->GetSecurityOrigin();
  return frame->GetPage()->GetPluginData(main_frame_origin);
}

DocumentInit& DocumentInit::WithTypeFrom(const String& mime_type) {
  mime_type_ = mime_type;
  type_ = ComputeDocumentType(GetFrame(), Url(), mime_type_,
                              &is_for_external_handler_);
  if (type_ == Type::kPlugin) {
    plugin_background_color_ =
        GetPluginData(GetFrame(), Url())
            ->PluginBackgroundColorForMimeType(mime_type_);
  }
  return *this;
}

DocumentInit& DocumentInit::WithExecutionContext(
    ExecutionContext* execution_context) {
  DCHECK(!execution_context_);
  DCHECK(!document_loader_);
#if DCHECK_IS_ON()
  DCHECK(!for_test_);
#endif
  execution_context_ = execution_context;
  content_security_policy_ = MakeGarbageCollected<ContentSecurityPolicy>();
  content_security_policy_->CopyStateFrom(
      execution_context_->GetContentSecurityPolicy());
  return *this;
}

DocumentInit& DocumentInit::WithURL(const KURL& url) {
  DCHECK(url_.IsNull());
  url_ = url;
  return *this;
}

void DocumentInit::CalculateAndCacheDocumentOrigin() {
  DCHECK(!cached_document_origin_);
  cached_document_origin_ = GetDocumentOrigin();
}

scoped_refptr<SecurityOrigin> DocumentInit::GetDocumentOrigin() const {
  if (cached_document_origin_)
    return cached_document_origin_;

  scoped_refptr<SecurityOrigin> document_origin;
  if (origin_to_commit_) {
    // Origin to commit is specified by the browser process, it must be taken
    // and used directly. It is currently supplied only for session history
    // navigations, where the origin was already calcuated previously and
    // stored on the session history entry.
    document_origin = origin_to_commit_;
  } else if (IsPagePopupRunningInWebTest(GetFrame())) {
    // If we are a page popup in LayoutTests ensure we use the popup
    // owner's security origin so the tests can possibly access the
    // document via internals API.
    document_origin = GetFrame()
                          ->PagePopupOwner()
                          ->GetDocument()
                          .GetSecurityOrigin()
                          ->IsolatedCopy();
  } else if (owner_document_) {
    document_origin = owner_document_->GetMutableSecurityOrigin();
  } else {
    // Otherwise, create an origin that propagates precursor information
    // as needed. For non-opaque origins, this creates a standard tuple
    // origin, but for opaque origins, it creates an origin with the
    // initiator origin as the precursor.
    document_origin = SecurityOrigin::CreateWithReferenceOrigin(
        url_, initiator_origin_.get());
  }

  if (IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin)) {
    auto sandbox_origin = document_origin->DeriveNewOpaqueOrigin();

    // If we're supposed to inherit our security origin from our
    // owner, but we're also sandboxed, the only things we inherit are
    // the origin's potential trustworthiness and the ability to
    // load local resources. The latter lets about:blank iframes in
    // file:// URL documents load images and other resources from
    // the file system.
    //
    // Note: Sandboxed about:srcdoc iframe without "allow-same-origin" aren't
    // allowed to load user's file, even if its parent can.
    if (owner_document_) {
      if (document_origin->IsPotentiallyTrustworthy())
        sandbox_origin->SetOpaqueOriginIsPotentiallyTrustworthy(true);
      if (document_origin->CanLoadLocalResources() && !IsSrcdocDocument())
        sandbox_origin->GrantLoadLocalResources();
    }
    document_origin = sandbox_origin;
  }

  if (TreeRootDocumentLoader() &&
      TreeRootDocumentLoader()->GetFrame()->GetSettings()) {
    Settings* settings = TreeRootDocumentLoader()->GetFrame()->GetSettings();
    if (!settings->GetWebSecurityEnabled()) {
      // Web security is turned off. We should let this document access
      // every other document. This is used primary by testing harnesses for
      // web sites.
      document_origin->GrantUniversalAccess();
    } else if (document_origin->IsLocal()) {
      if (settings->GetAllowUniversalAccessFromFileURLs()) {
        // Some clients want local URLs to have universal access, but that
        // setting is dangerous for other clients.
        document_origin->GrantUniversalAccess();
      } else if (!settings->GetAllowFileAccessFromFileURLs()) {
        // Some clients do not want local URLs to have access to other local
        // URLs.
        document_origin->BlockLocalAccessFromLocalOrigin();
      }
    }
  }

  if (grant_load_local_resources_)
    document_origin->GrantLoadLocalResources();

  if (document_origin->IsOpaque() && ShouldSetURL()) {
    KURL url = url_.IsEmpty() ? BlankURL() : url_;
    if (SecurityOrigin::Create(url)->IsPotentiallyTrustworthy())
      document_origin->SetOpaqueOriginIsPotentiallyTrustworthy(true);
  }
  return document_origin;
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

ExecutionContext* DocumentInit::GetExecutionContext() const {
  if (execution_context_)
    return execution_context_;
  return GetFrame() ? GetFrame()->DomWindow() : nullptr;
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
    network::mojom::blink::WebSandboxFlags flags) {
  // Only allow adding more sandbox flags.
  sandbox_flags_ |= flags;
  return *this;
}

ContentSecurityPolicy* DocumentInit::GetContentSecurityPolicy() const {
  DCHECK(content_security_policy_);
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

bool DocumentInit::ShouldReuseDOMWindow() const {
  DCHECK(GetFrame());
  // Secure transitions can only happen when navigating from the initial empty
  // document.
  if (!GetFrame()->Loader().StateMachine()->IsDisplayingInitialEmptyDocument())
    return false;
  return GetFrame()->GetDocument()->GetSecurityOrigin()->CanAccess(
      GetDocumentOrigin().get());
}

bool DocumentInit::IsSandboxed(
    network::mojom::blink::WebSandboxFlags mask) const {
  return (GetSandboxFlags() & mask) !=
         network::mojom::blink::WebSandboxFlags::kNone;
}

Document* DocumentInit::CreateDocument() const {
#if DCHECK_IS_ON()
  DCHECK(document_loader_ || execution_context_ || for_test_);
#endif
  switch (type_) {
    case Type::kHTML:
      return MakeGarbageCollected<HTMLDocument>(*this);
    case Type::kXHTML:
      return XMLDocument::CreateXHTML(*this);
    case Type::kImage:
      return MakeGarbageCollected<ImageDocument>(*this);
    case Type::kPlugin: {
      if (IsSandboxed(network::mojom::blink::WebSandboxFlags::kPlugins))
        return MakeGarbageCollected<SinkDocument>(*this);
      return MakeGarbageCollected<PluginDocument>(*this);
    }
    case Type::kMedia:
      return MakeGarbageCollected<MediaDocument>(*this);
    case Type::kSVG:
      return XMLDocument::CreateSVG(*this);
    case Type::kXML:
      return MakeGarbageCollected<XMLDocument>(*this);
    case Type::kViewSource:
      return MakeGarbageCollected<HTMLViewSourceDocument>(*this);
    case Type::kText:
      return MakeGarbageCollected<TextDocument>(*this);
    case Type::kUnspecified:
      FALLTHROUGH;
    default:
      break;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
