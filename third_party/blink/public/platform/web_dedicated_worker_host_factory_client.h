// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_fetch_client_settings_object.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
struct CrossOriginEmbedderPolicy;
}

namespace blink {

class WebURL;
class WebWorkerFetchContext;

// WebDedicatedWorkerHostFactoryClient is the interface to access
// content::DedicatedWorkerHostFactoryClient from blink::DedicatedWorker.
class WebDedicatedWorkerHostFactoryClient {
 public:
  virtual ~WebDedicatedWorkerHostFactoryClient() = default;

  // Requests the creation of DedicatedWorkerHost in the browser process.
  // For non-PlzDedicatedWorker. This will be removed once PlzDedicatedWorker is
  // enabled by default. This code is called by Blink so it wants to use the
  // Blink variant of DedicatedWorker and can't use the non-Blink variant.
  // TODO(chrisha): Unfortunately, this header is part of the blink_headers
  //     target, which itself is a dependency of blink Mojom targets, so this
  //     creates a dependency cycle. To break this cycle the token is passed as
  //     an untyped UnguessableToken through this interface. The implementation
  //     of this interface should immediately cast this back to a
  //     DedicatedWorkerToken! Break this dependency cycle and keep strong
  //     typing by introducing a non-Mojo concrete type for the token, and use
  //     Mojo bindings to convert to and from it.
  virtual void CreateWorkerHostDeprecated(
      const base::UnguessableToken& dedicated_worker_token,
      base::OnceCallback<void(const network::CrossOriginEmbedderPolicy&)>
          callback) = 0;
  // For PlzDedicatedWorker.
  virtual void CreateWorkerHost(
      const base::UnguessableToken& dedicated_worker_token,
      const blink::WebURL& script_url,
      network::mojom::CredentialsMode credentials_mode,
      const blink::WebFetchClientSettingsObject& fetch_client_settings_object,
      CrossVariantMojoRemote<mojom::BlobURLTokenInterfaceBase>
          blob_url_token) = 0;

  // Clones the given WebWorkerFetchContext for nested workers.
  virtual scoped_refptr<WebWorkerFetchContext> CloneWorkerFetchContext(
      WebWorkerFetchContext*,
      scoped_refptr<base::SingleThreadTaskRunner>) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
