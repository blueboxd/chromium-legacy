// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_manager_impl.h"
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
    : conversion_manager_provider_(
          std::make_unique<ConversionManagerProviderImpl>()),
      web_contents_(web_contents),
      receiver_(web_contents, this) {}

ConversionHost::~ConversionHost() = default;

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
      conversion_manager_provider_->GetManager(web_contents_);
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
