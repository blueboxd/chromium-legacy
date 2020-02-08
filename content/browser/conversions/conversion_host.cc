// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_policy.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"

namespace content {

ConversionHost::ConversionHost(WebContents* contents)
    : web_contents_(contents), receiver_(contents, this) {}

ConversionHost::~ConversionHost() = default;

// TODO(https://crbug.com/1044099): Limit the number of conversion redirects per
// page-load to a reasonable number.
void ConversionHost::RegisterConversion(
    blink::mojom::ConversionPtr conversion) {
  // If there is no conversion manager available, ignore any conversion
  // registrations.
  if (!GetManager())
    return;
  content::RenderFrameHost* render_frame_host =
      receiver_.GetCurrentTargetFrame();

  // Conversion registration is only allowed in the main frame.
  if (render_frame_host->GetParent()) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used by the main frame.");
    return;
  }

  // Only allow conversion registration on secure pages with a secure conversion
  // redirects.
  if (!network::IsOriginPotentiallyTrustworthy(
          render_frame_host->GetLastCommittedOrigin()) ||
      !network::IsOriginPotentiallyTrustworthy(conversion->reporting_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used in secure contexts with a "
        "secure conversion registration origin.");
    return;
  }

  StorableConversion storable_conversion(
      GetManager()->GetConversionPolicy().GetSanitizedConversionData(
          conversion->conversion_data),
      render_frame_host->GetLastCommittedOrigin(),
      conversion->reporting_origin);

  GetManager()->HandleConversion(storable_conversion);
}

void ConversionHost::SetCurrentTargetFrameForTesting(
    RenderFrameHost* render_frame_host) {
  receiver_.SetCurrentTargetFrameForTesting(render_frame_host);
}

ConversionManager* ConversionHost::GetManager() {
  return static_cast<StoragePartitionImpl*>(
             BrowserContext::GetDefaultStoragePartition(
                 web_contents_->GetBrowserContext()))
      ->GetConversionManager();
}

}  // namespace content
