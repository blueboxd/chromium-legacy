// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders.h"

#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/content_settings/content_settings_manager_impl.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/insecure_sensitive_input_driver_factory.h"
#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/blink/public/mojom/insecure_input/insecure_input_service.mojom.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/public_buildflags.h"

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "chrome/browser/android/contextualsearch/unhandled_tap_notifier_impl.h"
#include "chrome/browser/android/contextualsearch/unhandled_tap_web_contents_observer.h"
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom.h"
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

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

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
void BindUnhandledTapWebContentsObserver(
    content::RenderFrameHost* const host,
    mojo::PendingReceiver<blink::mojom::UnhandledTapNotifier> receiver) {
  auto* unhandled_tap_notifier_observer =
      contextual_search::UnhandledTapWebContentsObserver::FromWebContents(
          content::WebContents::FromRenderFrameHost(host));
  contextual_search::CreateUnhandledTapNotifierImpl(
      unhandled_tap_notifier_observer->device_scale_factor(),
      unhandled_tap_notifier_observer->unhandled_tap_callback(),
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

// Forward image Annotator requests to the profile's AccessibilityLabelsService.
void BindImageAnnotator(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver) {
  AccessibilityLabelsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(
          frame_host->GetProcess()->GetBrowserContext()))
      ->BindImageAnnotator(std::move(receiver));
}

void BindDistillabilityService(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<dom_distiller::mojom::DistillabilityService>
        receiver) {
  dom_distiller::DistillabilityDriver* driver =
      dom_distiller::DistillabilityDriver::FromWebContents(
          content::WebContents::FromRenderFrameHost(frame_host));
  if (!driver)
    return;
  driver->CreateDistillabilityService(std::move(receiver));
}

#if defined(OS_ANDROID)
template <typename Interface>
void ForwardToJavaWebContents(content::RenderFrameHost* frame_host,
                              mojo::PendingReceiver<Interface> receiver) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  if (contents)
    contents->GetJavaInterfaces()->GetInterface(std::move(receiver));
}

template <typename Interface>
void ForwardToJavaFrame(content::RenderFrameHost* render_frame_host,
                        mojo::PendingReceiver<Interface> receiver) {
  render_frame_host->GetJavaInterfaces()->GetInterface(std::move(receiver));
}
#endif

void PopulateChromeFrameBinders(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<mojom::ContentSettingsManager>(
      base::BindRepeating(&ContentSettingsManagerImpl::Create));

  map->Add<image_annotation::mojom::Annotator>(
      base::BindRepeating(&BindImageAnnotator));

  map->Add<blink::mojom::AnchorElementMetricsHost>(
      base::BindRepeating(&NavigationPredictor::Create));

  map->Add<blink::mojom::InsecureInputService>(
      base::BindRepeating(&InsecureSensitiveInputDriverFactory::BindDriver));

  map->Add<dom_distiller::mojom::DistillabilityService>(
      base::BindRepeating(&BindDistillabilityService));

#if defined(OS_ANDROID)
  map->Add<blink::mojom::InstalledAppProvider>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::InstalledAppProvider>));
#if defined(BROWSER_MEDIA_CONTROLS_MENU)
  map->Add<blink::mojom::MediaControlsMenuHost>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::MediaControlsMenuHost>));
#endif
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(base::BindRepeating(
        &ForwardToJavaFrame<payments::mojom::PaymentRequest>));
  }
  map->Add<blink::mojom::ShareService>(base::BindRepeating(
      &ForwardToJavaWebContents<blink::mojom::ShareService>));

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
  map->Add<blink::mojom::UnhandledTapNotifier>(
      base::BindRepeating(&BindUnhandledTapWebContentsObserver));
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

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
