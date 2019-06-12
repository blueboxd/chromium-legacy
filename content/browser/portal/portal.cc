// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal.h"

#include <unordered_map>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer_type_converters.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {
using PortalTokenMap = std::
    unordered_map<base::UnguessableToken, Portal*, base::UnguessableTokenHash>;
base::LazyInstance<PortalTokenMap>::Leaky g_portal_token_map =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

Portal::Portal(RenderFrameHostImpl* owner_render_frame_host)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(owner_render_frame_host)),
      owner_render_frame_host_(owner_render_frame_host),
      portal_token_(base::UnguessableToken::Create()),
      portal_host_receiver_(this) {
  auto pair = g_portal_token_map.Get().emplace(portal_token_, this);
  DCHECK(pair.second);
}

Portal::~Portal() {
  WebContentsImpl* outer_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));
  devtools_instrumentation::PortalDetached(outer_contents_impl->GetMainFrame());

  g_portal_token_map.Get().erase(portal_token_);
}

// static
bool Portal::IsEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kPortals) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

// static
Portal* Portal::FromToken(const base::UnguessableToken& portal_token) {
  PortalTokenMap& portals = g_portal_token_map.Get();
  auto it = portals.find(portal_token);
  return it == portals.end() ? nullptr : it->second;
}

// static
Portal* Portal::Create(
    RenderFrameHostImpl* owner_render_frame_host,
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client) {
  auto portal_ptr = base::WrapUnique(new Portal(owner_render_frame_host));
  Portal* portal = portal_ptr.get();
  portal->binding_ = mojo::MakeStrongAssociatedBinding<blink::mojom::Portal>(
      std::move(portal_ptr), std::move(receiver));
  portal->client_.Bind(std::move(client));
  return portal;
}

// static
std::unique_ptr<Portal> Portal::CreateForTesting(
    RenderFrameHostImpl* owner_render_frame_host) {
  return base::WrapUnique(new Portal(owner_render_frame_host));
}

// static
void Portal::BindPortalHostReceiver(
    RenderFrameHostImpl* frame,
    mojo::PendingAssociatedReceiver<blink::mojom::PortalHost>
        pending_receiver) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(frame));

  // This guards against the blink::mojom::PortalHost interface being used
  // outside the main frame of a Portal's guest.
  if (!web_contents || !web_contents->IsPortal() ||
      !frame->frame_tree_node()->IsMainFrame()) {
    mojo::ReportBadMessage(
        "blink.mojom.PortalHost can only be used by the the main frame of a "
        "Portal's guest.");
    return;
  }

  // This binding may already be bound to another request, and in such cases,
  // we rebind with the new request. An example scenario is a new document after
  // a portal navigation trying to create a connection, but the old document
  // hasn't been destroyed yet (and the pipe hasn't been closed).
  auto& receiver = web_contents->portal()->portal_host_receiver_;
  if (receiver.is_bound())
    receiver.reset();
  receiver.Bind(std::move(pending_receiver));
}

RenderFrameProxyHost* Portal::CreateProxyAndAttachPortal() {
  WebContentsImpl* outer_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));

  service_manager::mojom::InterfaceProviderPtr interface_provider;
  auto interface_provider_request(mojo::MakeRequest(&interface_provider));

  blink::mojom::DocumentInterfaceBrokerPtrInfo
      document_interface_broker_content;
  blink::mojom::DocumentInterfaceBrokerPtrInfo document_interface_broker_blink;

  // Create a FrameTreeNode in the outer WebContents to host the portal, in
  // response to the creation of a portal in the renderer process.
  FrameTreeNode* outer_node = outer_contents_impl->GetFrameTree()->AddFrame(
      owner_render_frame_host_->frame_tree_node(),
      owner_render_frame_host_->GetProcess()->GetID(),
      owner_render_frame_host_->GetProcess()->GetNextRoutingID(),
      std::move(interface_provider_request),
      mojo::MakeRequest(&document_interface_broker_content),
      mojo::MakeRequest(&document_interface_broker_blink),
      blink::WebTreeScopeType::kDocument, "", "", true,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), false, blink::FrameOwnerElementType::kPortal);

  bool web_contents_created = false;
  if (!portal_contents_) {
    // Create the Portal WebContents.
    WebContents::CreateParams params(outer_contents_impl->GetBrowserContext());
    SetPortalContents(WebContents::Create(params));
    web_contents_created = true;
  }

  DCHECK_EQ(portal_contents_.get(), portal_contents_impl_);
  DCHECK_EQ(portal_contents_impl_->portal(), this);
  DCHECK_EQ(portal_contents_impl_->GetDelegate(), this);

  outer_contents_impl->AttachInnerWebContents(std::move(portal_contents_),
                                              outer_node->current_frame_host());

  FrameTreeNode* frame_tree_node =
      portal_contents_impl_->GetMainFrame()->frame_tree_node();
  RenderFrameProxyHost* proxy_host =
      frame_tree_node->render_manager()->GetProxyToOuterDelegate();
  proxy_host->set_render_frame_proxy_created(true);
  portal_contents_impl_->ReattachToOuterWebContentsFrame();

  if (web_contents_created)
    PortalWebContentsCreated(portal_contents_impl_);

  devtools_instrumentation::PortalAttached(outer_contents_impl->GetMainFrame());

  return proxy_host;
}

void Portal::Navigate(const GURL& url, blink::mojom::ReferrerPtr referrer) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    mojo::ReportBadMessage("Portal::Navigate tried to use non-HTTP protocol.");
    binding_->Close();  // Also deletes |this|.
    return;
  }

  // TODO(lfg): Investigate which other restrictions we might need when
  // navigating portals. See http://crbug.com/964395.

  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.referrer = mojo::ConvertTo<Referrer>(referrer);
  portal_contents_impl_->GetController().LoadURLWithParams(load_url_params);
}

void Portal::Activate(blink::TransferableMessage data,
                      ActivateCallback callback) {
  WebContentsImpl* outer_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));

  if (outer_contents->portal()) {
    mojo::ReportBadMessage("Portal::Activate called on nested portal");
    binding_->Close();  // Also deletes |this|.
    return;
  }

  WebContentsDelegate* delegate = outer_contents->GetDelegate();
  bool is_loading = portal_contents_impl_->IsLoading();
  FrameTreeNode* outer_frame_tree_node = FrameTreeNode::GloballyFindByID(
      portal_contents_impl_->GetOuterDelegateFrameTreeNodeId());
  std::unique_ptr<WebContents> portal_contents =
      portal_contents_impl_->DetachFromOuterWebContents();
  owner_render_frame_host_->RemoveChild(outer_frame_tree_node);

  auto* outer_contents_main_frame_view = static_cast<RenderWidgetHostViewBase*>(
      outer_contents->GetMainFrame()->GetView());

  if (outer_contents_main_frame_view) {
    // Take fallback contents from previous WebContents so that the activation
    // is smooth without flashes.
    auto* portal_contents_main_frame_view =
        static_cast<RenderWidgetHostViewBase*>(
            portal_contents_impl_->GetMainFrame()->GetView());
    portal_contents_main_frame_view->TakeFallbackContentFrom(
        outer_contents_main_frame_view);

    outer_contents_main_frame_view->Destroy();
  }

  std::unique_ptr<WebContents> predecessor_web_contents =
      delegate->SwapWebContents(outer_contents, std::move(portal_contents),
                                true, is_loading);
  CHECK_EQ(predecessor_web_contents.get(), outer_contents);

  portal_contents_impl_->set_portal(nullptr);

  mojo::PendingAssociatedRemote<blink::mojom::Portal> pending_portal;
  auto portal_receiver = pending_portal.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::PortalClient> pending_client;
  auto client_receiver = pending_client.InitWithNewEndpointAndPassReceiver();

  Portal* portal =
      Create(portal_contents_impl_->GetMainFrame(), std::move(portal_receiver),
             std::move(pending_client));
  portal->SetPortalContents(std::move(predecessor_web_contents));

  portal_contents_impl_->GetMainFrame()->OnPortalActivated(
      portal->portal_token_, std::move(pending_portal),
      std::move(client_receiver), std::move(data), std::move(callback));

  devtools_instrumentation::PortalActivated(outer_contents->GetMainFrame());
}

void Portal::PostMessageToGuest(
    blink::TransferableMessage message,
    const base::Optional<url::Origin>& target_origin) {
  portal_contents_impl_->GetMainFrame()->ForwardMessageFromHost(
      std::move(message), owner_render_frame_host_->GetLastCommittedOrigin(),
      target_origin);
}

void Portal::PostMessageToHost(
    blink::TransferableMessage message,
    const base::Optional<url::Origin>& target_origin) {
  DCHECK(GetPortalContents());
  if (target_origin) {
    if (target_origin != owner_render_frame_host_->GetLastCommittedOrigin())
      return;
  }
  client().ForwardMessageFromGuest(
      std::move(message),
      GetPortalContents()->GetMainFrame()->GetLastCommittedOrigin(),
      target_origin);
}

void Portal::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  if (render_frame_host == owner_render_frame_host_)
    binding_->Close();  // Also deletes |this|.
}

void Portal::WebContentsDestroyed() {
  binding_->Close();  // Also deletes |this|.
}

void Portal::LoadingStateChanged(WebContents* source,
                                 bool to_different_document) {
  DCHECK_EQ(source, portal_contents_impl_);
  if (!source->IsLoading())
    client_->DispatchLoadEvent();
}

void Portal::PortalWebContentsCreated(WebContents* portal_web_contents) {
  WebContentsImpl* outer_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(owner_render_frame_host_));
  DCHECK(outer_contents->GetDelegate());
  outer_contents->GetDelegate()->PortalWebContentsCreated(portal_web_contents);
}

base::UnguessableToken Portal::GetDevToolsFrameToken() const {
  return portal_contents_impl_->GetMainFrame()->GetDevToolsFrameToken();
}

WebContentsImpl* Portal::GetPortalContents() {
  return portal_contents_impl_;
}

void Portal::SetBindingForTesting(
    mojo::StrongAssociatedBindingPtr<blink::mojom::Portal> binding) {
  binding_ = binding;
}

void Portal::SetClientForTesting(
    mojo::AssociatedRemote<blink::mojom::PortalClient> client) {
  client_ = std::move(client);
}

void Portal::SetPortalContents(std::unique_ptr<WebContents> web_contents) {
  portal_contents_ = std::move(web_contents);
  portal_contents_impl_ = static_cast<WebContentsImpl*>(portal_contents_.get());
  portal_contents_impl_->SetDelegate(this);
  portal_contents_impl_->set_portal(this);
}

}  // namespace content
