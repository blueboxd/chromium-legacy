// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_
#define CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_

#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/origin.h"

namespace content {

class RenderProcessHost;
struct WebPreferences;

// URLLoaderFactoryParamsHelper encapsulates details of how to create
// network::mojom::URLLoaderFactoryParams (taking //content-focused parameters,
// calling into ContentBrowserClient's OverrideURLLoaderFactoryParams method,
// etc.)
class URLLoaderFactoryParamsHelper {
 public:
  static network::mojom::URLLoaderFactoryParamsPtr Create(
      RenderProcessHost* process,
      const url::Origin& frame_origin,
      const base::UnguessableToken& top_frame_token,
      const net::NetworkIsolationKey& network_isolation_key,
      network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
      const WebPreferences& preferences);

  static network::mojom::URLLoaderFactoryParamsPtr CreateForIsolatedWorld(
      RenderProcessHost* process,
      const url::Origin& isolated_world_origin,
      const url::Origin& main_world_origin,
      const base::UnguessableToken& top_frame_token,
      const net::NetworkIsolationKey& network_isolation_key,
      network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
      const WebPreferences& preferences);

  static network::mojom::URLLoaderFactoryParamsPtr CreateForPrefetch(
      RenderProcessHost* process,
      const url::Origin& frame_origin,
      const base::UnguessableToken& top_frame_token,
      network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy,
      const WebPreferences& preferences);

  static network::mojom::URLLoaderFactoryParamsPtr CreateForWorker(
      RenderProcessHost* process,
      const url::Origin& worker_origin);

  // TODO(kinuko, lukasza): https://crbug.com/891872: Remove, once all
  // URLLoaderFactories are associated with a specific execution context (e.g. a
  // frame, a service worker or any other kind of worker).
  static network::mojom::URLLoaderFactoryParamsPtr CreateForRendererProcess(
      RenderProcessHost* process);

 private:
  // Only static methods.
  URLLoaderFactoryParamsHelper() = delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_URL_LOADER_FACTORY_PARAMS_HELPER_H_
