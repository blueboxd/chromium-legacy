// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_params_helper.h"

#include "base/command_line.h"
#include "base/optional.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "ipc/ipc_message.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {

// Helper used by the public URLLoaderFactoryParamsHelper::Create... methods.
//
// |origin| is the origin that will use the URLLoaderFactory.
// |origin| is typically the same as the origin in
// network::ResourceRequest::request_initiator, except when
// |is_for_isolated_world|.  See also the doc comment for
// extensions::URLLoaderFactoryManager::CreateFactory.
//
// TODO(kinuko, lukasza): https://crbug.com/891872: Make
// |request_initiator_site_lock| non-optional, once
// URLLoaderFactoryParamsHelper::CreateForRendererProcess is removed.
network::mojom::URLLoaderFactoryParamsPtr CreateParams(
    RenderProcessHost* process,
    const url::Origin& origin,
    const base::Optional<url::Origin>& request_initiator_site_lock,
    const base::Optional<url::Origin>& top_frame_origin,
    bool is_trusted,
    const base::Optional<base::UnguessableToken>& top_frame_token,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    bool allow_universal_access_from_file_urls,
    bool is_for_isolated_world,
    bool is_for_service_worker,
    int frame_routing_id) {
  DCHECK(process);

  // "chrome-guest://..." is never used as a main or isolated world origin.
  DCHECK_NE(kGuestScheme, origin.scheme());
  DCHECK(!request_initiator_site_lock.has_value() ||
         request_initiator_site_lock->scheme() != kGuestScheme);

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();

  params->process_id = process->GetID();
  params->request_initiator_site_lock = request_initiator_site_lock;
  params->top_frame_origin = top_frame_origin;

  params->is_trusted = is_trusted;
  params->top_frame_id = top_frame_token;
  params->isolation_info = isolation_info;

  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);
  params->client_security_state = std::move(client_security_state);
  params->coep_reporter = std::move(coep_reporter);

  if (params->disable_web_security) {
    // --disable-web-security also disables Cross-Origin Read Blocking (CORB).
    params->is_corb_enabled = false;
  } else if (allow_universal_access_from_file_urls &&
             origin.scheme() == url::kFileScheme) {
    // allow_universal_access_from_file_urls disables CORB (via
    // |is_corb_enabled|) and CORS (via |disable_web_security|) for requests
    // made from a file: |origin|.
    params->is_corb_enabled = false;
    params->disable_web_security = true;
  } else {
    params->is_corb_enabled = true;
  }

  GetContentClient()->browser()->OverrideURLLoaderFactoryParams(
      process->GetBrowserContext(), origin, is_for_isolated_world,
      params.get());

  params->cookie_observer =
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition())
          ->CreateCookieAccessObserver(is_for_service_worker, process->GetID(),
                                       frame_routing_id);

  return params;
}

}  // namespace

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForFrame(
    RenderFrameHostImpl* frame,
    const url::Origin& frame_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    RenderProcessHost* process) {
  return CreateParams(
      process,
      frame_origin,  // origin
      frame_origin,  // request_initiator_site_lock
      // top_frame_origin
      frame->ComputeTopFrameOrigin(frame_origin),
      false,  // is_trusted
      frame->GetTopFrameToken(), frame->GetIsolationInfoForSubresources(),
      std::move(client_security_state), std::move(coep_reporter),
      frame->GetRenderViewHost()
          ->GetWebkitPreferences()
          .allow_universal_access_from_file_urls,
      false,  // is_for_isolated_world
      false,  // is_for_service_worker
      frame->GetRoutingID());
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForIsolatedWorld(
    RenderFrameHostImpl* frame,
    const url::Origin& isolated_world_origin,
    const url::Origin& main_world_origin,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  return CreateParams(frame->GetProcess(),
                      isolated_world_origin,  // origin
                      main_world_origin,      // request_initiator_site_lock
                      base::nullopt,          // top_frame_origin
                      false,                  // is_trusted
                      frame->GetTopFrameToken(),
                      frame->GetIsolationInfoForSubresources(),
                      std::move(client_security_state),
                      mojo::NullRemote(),  // coep_reporter
                      frame->GetRenderViewHost()
                          ->GetWebkitPreferences()
                          .allow_universal_access_from_file_urls,
                      true,   // is_for_isolated_world
                      false,  // is_for_service_worker
                      frame->GetRoutingID());
}

network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForPrefetch(
    RenderFrameHostImpl* frame,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  // The factory client |is_trusted| to control the |network_isolation_key| in
  // each separate request (rather than forcing the client to use the key
  // specified in URLLoaderFactoryParams).
  const url::Origin& frame_origin = frame->GetLastCommittedOrigin();
  return CreateParams(frame->GetProcess(),
                      frame_origin,  // origin
                      frame_origin,  // request_initiator_site_lock
                      // top_frame_origin
                      frame->ComputeTopFrameOrigin(frame_origin),
                      true,  // is_trusted
                      frame->GetTopFrameToken(),
                      net::IsolationInfo(),  // isolation_info
                      std::move(client_security_state),
                      mojo::NullRemote(),  // coep_reporter
                      frame->GetRenderViewHost()
                          ->GetWebkitPreferences()
                          .allow_universal_access_from_file_urls,
                      false,  // is_for_isolated_world
                      false,  // is_for_service_worker
                      frame->GetRoutingID());
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForWorker(
    RenderProcessHost* process,
    const url::Origin& request_initiator,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  return CreateParams(process,
                      request_initiator,  // origin
                      request_initiator,  // request_initiator_site_lock
                      base::nullopt,      // top_frame_origin
                      false,              // is_trusted
                      base::nullopt,      // top_frame_token
                      isolation_info,
                      nullptr,  // client_security_state
                      std::move(coep_reporter),
                      false,  // allow_universal_access_from_file_urls
                      false,  // is_for_isolated_world
                      false,  // is_for_service_worker
                      MSG_ROUTING_NONE);
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForRendererProcess(
    RenderProcessHost* process) {
  // Attempt to use the process lock as |request_initiator_site_lock|.
  base::Optional<url::Origin> request_initiator_site_lock;
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  GURL process_lock = policy->GetOriginLock(process->GetID());
  if (process_lock.is_valid()) {
    request_initiator_site_lock =
        SiteInstanceImpl::GetRequestInitiatorSiteLock(process_lock);
  }

  // Since this function is about to get deprecated (crbug.com/891872), it
  // should be fine to not add support for isolation info thus using an empty
  // NetworkIsolationKey.
  //
  // We may not be able to allow powerful APIs such as memory measurement APIs
  // (see https://crbug.com/887967) without removing this call.
  net::IsolationInfo isolation_info;
  base::Optional<base::UnguessableToken> top_frame_token = base::nullopt;

  return CreateParams(
      process,
      url::Origin(),                // origin
      request_initiator_site_lock,  // request_initiator_site_lock
      base::nullopt,                // top_frame_origin
      false,                        // is_trusted
      top_frame_token, isolation_info,
      nullptr,             // client_security_state
      mojo::NullRemote(),  // coep_reporter
      false,               // allow_universal_access_from_file_urls
      false,               // is_for_isolated_world
      false,               // is_for_service_worker
      MSG_ROUTING_NONE);
}

}  // namespace content
