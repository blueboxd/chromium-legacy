// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_params_helper.h"

#include "base/command_line.h"
#include "base/optional.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
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
// CreateURLLoaderFactoryForRendererProcess is removed.
network::mojom::URLLoaderFactoryParamsPtr CreateParams(
    RenderProcessHost* process,
    const url::Origin& origin,
    const base::Optional<url::Origin>& request_initiator_site_lock,
    bool is_trusted,
    const base::Optional<base::UnguessableToken>& top_frame_token,
    const base::Optional<net::NetworkIsolationKey>& network_isolation_key,
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
    bool allow_universal_access_from_file_urls,
    bool is_for_isolated_world) {
  DCHECK(process);

  // "chrome-guest://..." is never used as a main or isolated world origin.
  DCHECK_NE(kGuestScheme, origin.scheme());
  DCHECK(!request_initiator_site_lock.has_value() ||
         request_initiator_site_lock->scheme() != kGuestScheme);

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();

  params->process_id = process->GetID();
  params->request_initiator_site_lock = request_initiator_site_lock;

  params->is_trusted = is_trusted;
  params->top_frame_id = top_frame_token;
  params->network_isolation_key = network_isolation_key;

  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);
  params->cross_origin_embedder_policy = cross_origin_embedder_policy;

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

  return params;
}

}  // namespace

// static
network::mojom::URLLoaderFactoryParamsPtr URLLoaderFactoryParamsHelper::Create(
    RenderProcessHost* process,
    const url::Origin& frame_origin,
    const base::UnguessableToken& top_frame_token,
    const net::NetworkIsolationKey& network_isolation_key,
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
    const WebPreferences& preferences) {
  return CreateParams(process,
                      frame_origin,  // origin
                      frame_origin,  // request_initiator_site_lock
                      false,         // is_trusted
                      top_frame_token, network_isolation_key,
                      cross_origin_embedder_policy,
                      preferences.allow_universal_access_from_file_urls,
                      false);  // is_for_isolated_world
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForIsolatedWorld(
    RenderProcessHost* process,
    const url::Origin& isolated_world_origin,
    const url::Origin& main_world_origin,
    const base::UnguessableToken& top_frame_token,
    const net::NetworkIsolationKey& network_isolation_key,
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
    const WebPreferences& preferences) {
  return CreateParams(process,
                      isolated_world_origin,  // origin
                      main_world_origin,      // request_initiator_site_lock
                      false,                  // is_trusted
                      top_frame_token, network_isolation_key,
                      cross_origin_embedder_policy,
                      preferences.allow_universal_access_from_file_urls,
                      true);  // is_for_isolated_world
}

network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForPrefetch(
    RenderProcessHost* process,
    const url::Origin& frame_origin,
    const base::UnguessableToken& top_frame_token,
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
    const WebPreferences& preferences) {
  return CreateParams(process,
                      frame_origin,  // origin
                      frame_origin,  // request_initiator_site_lock
                      true,          // is_trusted
                      top_frame_token,
                      base::nullopt,  // network_isolation_key
                      cross_origin_embedder_policy,
                      preferences.allow_universal_access_from_file_urls,
                      false);  // is_for_isolated_world
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForWorker(
    RenderProcessHost* process,
    const url::Origin& worker_origin) {
  net::NetworkIsolationKey network_isolation_key(
      worker_origin,   // top_frame_origin
      worker_origin);  // frame_origin

  return CreateParams(process,
                      worker_origin,  // origin
                      worker_origin,  // request_initiator_site_lock
                      false,          // is_trusted
                      base::nullopt,  // top_frame_token
                      network_isolation_key,
                      network::mojom::CrossOriginEmbedderPolicy::kNone,
                      false,   // allow_universal_access_from_file_urls
                      false);  // is_for_isolated_world
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
  // should be fine to not add support for network isolation thus sending empty
  // key.
  //
  // We may not be able to allow powerful APIs such as memory measurement APIs
  // (see https://crbug.com/887967) without removing this call.
  base::Optional<net::NetworkIsolationKey> network_isolation_key =
      base::nullopt;
  base::Optional<base::UnguessableToken> top_frame_token = base::nullopt;

  return CreateParams(
      process,
      url::Origin(),                // origin
      request_initiator_site_lock,  // request_initiator_site_lock
      false,                        // is_trusted
      top_frame_token, network_isolation_key,
      network::mojom::CrossOriginEmbedderPolicy::kNone,
      false,   // allow_universal_access_from_file_urls
      false);  // is_for_isolated_world
}

}  // namespace content
