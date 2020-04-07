// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_policy.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"

namespace content {

namespace {

// Provides access to the manager owned by the default storage partition.
class ConversionManagerProviderImpl : public ConversionManager::Provider {
 public:
  ConversionManagerProviderImpl() = default;
  ConversionManagerProviderImpl(const ConversionManagerProviderImpl& other) =
      delete;
  ConversionManagerProviderImpl& operator=(
      const ConversionManagerProviderImpl& other) = delete;
  ~ConversionManagerProviderImpl() override = default;

  // ConversionManagerProvider:
  ConversionManager* GetManager(WebContents* web_contents) const override {
    return static_cast<StoragePartitionImpl*>(
               BrowserContext::GetDefaultStoragePartition(
                   web_contents->GetBrowserContext()))
        ->GetConversionManager();
  }
};

}  // namespace

// static
std::unique_ptr<ConversionHost> ConversionHost::CreateForTesting(
    WebContents* web_contents,
    std::unique_ptr<ConversionManager::Provider> conversion_manager_provider) {
  auto host = std::make_unique<ConversionHost>(web_contents);
  host->conversion_manager_provider_ = std::move(conversion_manager_provider);
  return host;
}

ConversionHost::ConversionHost(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      conversion_manager_provider_(
          std::make_unique<ConversionManagerProviderImpl>()),
      receiver_(web_contents, this) {}

ConversionHost::~ConversionHost() = default;

void ConversionHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  ConversionManager* conversion_manager =
      conversion_manager_provider_->GetManager(web_contents());
  if (!conversion_manager)
    return;

  // Get the impression data off the navigation.
  if (!navigation_handle->GetImpression())
    return;
  const Impression& impression = *(navigation_handle->GetImpression());

  // If an impression is not associated with a main frame navigation, ignore it.
  // If the navigation did not commit, committed to a Chrome error page, or was
  // same document, ignore it. Impressions should never be attached to
  // same-document navigations but can be the result of a bad renderer.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      navigation_handle->IsSameDocument())
    return;

  // If the impression's conversion destination does not match the final top
  // frame origin of this new navigation ignore it.
  if (impression.conversion_destination !=
      navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin())
    return;

  // TODO(johnidel): When impression_origin is available, we should default to
  // it instead of conversion destination. We also need to verify that
  // impression actually occurred on a secure site.
  //
  // Convert |impression| into a StorableImpression that can be forwarded to
  // storage. If a reporting origin was not provided, default to the conversion
  // destination for reporting.
  const url::Origin& reporting_origin = !impression.reporting_origin
                                            ? impression.conversion_destination
                                            : *impression.reporting_origin;

  // Conversion measurement is only allowed in secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(reporting_origin) ||
      !network::IsOriginPotentiallyTrustworthy(
          impression.conversion_destination)) {
    // TODO (1049654): This should log a console error when it occurs.
    return;
  }

  base::Time impression_time = base::Time::Now();
  const ConversionPolicy& policy = conversion_manager->GetConversionPolicy();

  // TODO(https://crbug.com/1061645): The impression origin should be provided
  // by looking up the navigation initiator frame's top frame origin.
  StorableImpression storable_impression(
      policy.GetSanitizedImpressionData(impression.impression_data),
      url::Origin() /* impression_origin */, impression.conversion_destination,
      reporting_origin, impression_time,
      policy.GetExpiryTimeForImpression(impression.expiry, impression_time),
      /*impression_id=*/base::nullopt);

  conversion_manager->HandleImpression(storable_impression);
}

// TODO(https://crbug.com/1044099): Limit the number of conversion redirects per
// page-load to a reasonable number.
void ConversionHost::RegisterConversion(
    blink::mojom::ConversionPtr conversion) {
  content::RenderFrameHost* render_frame_host =
      receiver_.GetCurrentTargetFrame();

  // Conversion registration is only allowed in the main frame.
  if (render_frame_host->GetParent()) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used by the main frame.");
    return;
  }

  // If there is no conversion manager available, ignore any conversion
  // registrations.
  ConversionManager* conversion_manager =
      conversion_manager_provider_->GetManager(web_contents());
  if (!conversion_manager)
    return;

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
      conversion_manager->GetConversionPolicy().GetSanitizedConversionData(
          conversion->conversion_data),
      render_frame_host->GetLastCommittedOrigin(),
      conversion->reporting_origin);

  conversion_manager->HandleConversion(storable_conversion);
}

void ConversionHost::SetCurrentTargetFrameForTesting(
    RenderFrameHost* render_frame_host) {
  receiver_.SetCurrentTargetFrameForTesting(render_frame_host);
}

}  // namespace content
