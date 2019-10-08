// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/image_annotation/public/mojom/constants.mojom-forward.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

#if defined(OS_ANDROID)
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#if defined(ENABLE_SPATIAL_NAVIGATION_HOST)
#include "third_party/blink/public/mojom/page/spatial_navigation.mojom.h"
#endif
#else
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/payments/payment_request_factory.h"
#endif

namespace chrome {
namespace internal {

#if defined(OS_ANDROID)
template <typename Interface>
void ForwardToJavaWebContents(content::RenderFrameHost* frame_host,
                              mojo::PendingReceiver<Interface> receiver) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  if (contents)
    contents->GetJavaInterfaces()->GetInterface(std::move(receiver));
}
#endif

// Forward image Annotator requests to the image_annotation service.
void BindImageAnnotator(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver) {
  content::BrowserContext::GetConnectorFor(
      frame_host->GetProcess()->GetBrowserContext())
      ->Connect(image_annotation::mojom::kServiceName, std::move(receiver));
}

#if defined(OS_ANDROID)
template <typename Interface>
void ForwardToJavaFrameRegistry(content::RenderFrameHost* render_frame_host,
                                mojo::PendingReceiver<Interface> receiver) {
  render_frame_host->GetJavaInterfaces()->GetInterface(std::move(receiver));
}
#endif

void PopulateChromeFrameBinders(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<image_annotation::mojom::Annotator>(
      base::BindRepeating(&BindImageAnnotator));

  map->Add<blink::mojom::AnchorElementMetricsHost>(
      base::BindRepeating(&NavigationPredictor::Create));
#if defined(OS_ANDROID)
  map->Add<blink::mojom::InstalledAppProvider>(base::BindRepeating(
      &ForwardToJavaFrameRegistry<blink::mojom::InstalledAppProvider>));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(base::BindRepeating(
        &ForwardToJavaFrameRegistry<payments::mojom::PaymentRequest>));
  }
  map->Add<blink::mojom::ShareService>(base::BindRepeating(
      &ForwardToJavaWebContents<blink::mojom::ShareService>));
#if defined(ENABLE_SPATIAL_NAVIGATION_HOST)
  map->Add<blink::mojom::SpatialNavigationHost>(base::BindRepeating(
      &ForwardToJavaWebContents<blink::mojom::SpatialNavigationHost>));
#endif
#else
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&badging::BadgeManager::BindReceiver));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(
        base::BindRepeating(&payments::CreatePaymentRequest));
  }
#endif
}

}  // namespace internal
}  // namespace chrome
