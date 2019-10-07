// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FETCH_CLIENT_SETTINGS_OBJECT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FETCH_CLIENT_SETTINGS_OBJECT_H_

#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_url.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"  // nogncheck
#include "third_party/blink/renderer/platform/weborigin/kurl.h"  // nogncheck
#endif

namespace blink {

// Yet another variant of FetchClientSettingsObject. Having this is unfortunate
// but we need this struct for the interaction between blink and content until
// Onion Soup is done.
// Keep this struct consistent with mojom::FetchClientSettingsObject.
struct WebFetchClientSettingsObject {
  network::mojom::ReferrerPolicy referrer_policy;
  WebURL outgoing_referrer;
  mojom::InsecureRequestsPolicy insecure_requests_policy;

#if INSIDE_BLINK
  explicit WebFetchClientSettingsObject(
      const FetchClientSettingsObjectSnapshot& snapshot)
      : referrer_policy(snapshot.GetReferrerPolicy()),
        outgoing_referrer(KURL(snapshot.GetOutgoingReferrer())),
        insecure_requests_policy(
            snapshot.GetInsecureRequestsPolicy() &
                    blink::kUpgradeInsecureRequests
                ? blink::mojom::InsecureRequestsPolicy::kUpgrade
                : blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade) {}
#endif  // INSIDE_BLINK
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_FETCH_CLIENT_SETTINGS_OBJECT_H_
