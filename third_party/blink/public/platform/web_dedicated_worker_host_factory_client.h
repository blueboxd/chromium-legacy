// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class WebSecurityOrigin;
class WebURL;
class WebWorkerFetchContext;

// WebDedicatedWorkerHostFactoryClient is the interface to access
// content::DedicatedWorkerHostFactoryClient from blink::DedicatedWorker.
class WebDedicatedWorkerHostFactoryClient {
 public:
  virtual ~WebDedicatedWorkerHostFactoryClient() = default;

  // Requests the creation of DedicatedWorkerHost in the browser process.
  // For non-PlzDedicatedWorker. This will be removed once PlzDedicatedWorker is
  // enabled by default.
  virtual void CreateWorkerHostDeprecated(
      const blink::WebSecurityOrigin& script_origin) = 0;
  // For PlzDedicatedWorker.
  // TODO(nhiroki): Pack |fetch_client_*| into some struct like
  // WebFetchClientSettingsObject.
  virtual void CreateWorkerHost(
      const blink::WebURL& script_url,
      const blink::WebSecurityOrigin& script_origin,
      network::mojom::CredentialsMode credentials_mode,
      const blink::WebSecurityOrigin& fetch_client_security_origin,
      network::mojom::ReferrerPolicy fetch_client_referrer_policy,
      const blink::WebURL& fetch_client_outgoing_referrer,
      mojo::ScopedMessagePipeHandle blob_url_token) = 0;

  // Clones the given WebWorkerFetchContext for nested workers.
  virtual scoped_refptr<WebWorkerFetchContext> CloneWorkerFetchContext(
      WebWorkerFetchContext*,
      scoped_refptr<base::SingleThreadTaskRunner>) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
