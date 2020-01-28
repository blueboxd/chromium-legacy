// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders.h"

#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/language/translate_frame_binder.h"
#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/predictors/network_hints_handler_impl.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/insecure_sensitive_input_driver_factory.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_ui.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/media/media_history_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/common/prerender.mojom.h"
#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/feed/buildflags.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "components/translate/content/common/translate.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/blink/public/mojom/insecure_input/insecure_input_service.mojom.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/public_buildflags.h"

#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "chrome/browser/android/contextualsearch/unhandled_tap_notifier_impl.h"
#include "chrome/browser/android/contextualsearch/unhandled_tap_web_contents_observer.h"
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom.h"
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/browser/ui/webui/reset_password/reset_password_ui.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

#if defined(OS_ANDROID)
#include "chrome/browser/android/contextualsearch/contextual_search_observer.h"
#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals_ui.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals.mojom.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals_ui.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/contextual_search/content/browser/contextual_search_js_api_service_impl.h"
#include "components/contextual_search/content/common/mojom/contextual_search_js_api_service.mojom.h"
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
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_dialog.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer.mojom.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader.mojom.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.mojom.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/chromeos/network_ui.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"
#include "chromeos/components/multidevice/debug_webui/proximity_auth_ui.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
#include "chrome/browser/media/kaleidoscope/internal/kaleidoscope_ui.h"
#include "chrome/browser/media/kaleidoscope/internal/mojom/kaleidoscope.mojom.h"
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

namespace chrome {
namespace internal {

namespace {

template <typename Interface, int N, typename... Subclasses>
struct BinderHelper;

template <typename Interface, typename WebUIControllerSubclass>
bool SafeDownCastAndBindInterface(content::WebUI* web_ui,
                                  mojo::PendingReceiver<Interface>& receiver) {
  // Performs a safe downcast to the concrete WebUIController subclass.
  WebUIControllerSubclass* concrete_controller =
      web_ui ? web_ui->GetController()->GetAs<WebUIControllerSubclass>()
             : nullptr;

  if (!concrete_controller)
    return false;

  // Fails to compile if |Subclass| does not implement the appropriate overload
  // for |Interface|.
  concrete_controller->BindInterface(std::move(receiver));
  return true;
}

template <typename Interface, int N, typename Subclass, typename... Subclasses>
struct BinderHelper<Interface, N, std::tuple<Subclass, Subclasses...>> {
  static bool BindInterface(content::WebUI* web_ui,
                            mojo::PendingReceiver<Interface> receiver) {
    // Try a different subclass if the current one is not the right
    // WebUIController for the current WebUI page, and only fail if none of the
    // passed subclasses match.
    if (!SafeDownCastAndBindInterface<Interface, Subclass>(web_ui, receiver)) {
      return BinderHelper<Interface, N - 1, std::tuple<Subclasses...>>::
          BindInterface(web_ui, std::move(receiver));
    }
    return true;
  }
};

template <typename Interface, typename Subclass, typename... Subclasses>
struct BinderHelper<Interface, 0, std::tuple<Subclass, Subclasses...>> {
  static bool BindInterface(content::WebUI* web_ui,
                            mojo::PendingReceiver<Interface> receiver) {
    return SafeDownCastAndBindInterface<Interface, Subclass>(web_ui, receiver);
  }
};

// Registers a binder in |map| that binds |Interface| iff the RenderFrameHost
// has a WebUIController among type |WebUIControllerSubclasses|.
template <typename Interface, typename... WebUIControllerSubclasses>
void RegisterWebUIControllerInterfaceBinder(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<Interface>(
      base::BindRepeating([](content::RenderFrameHost* host,
                             mojo::PendingReceiver<Interface> receiver) {
        // This is expected to be called only for main frames.
        if (host->GetParent()) {
          ReceivedBadMessage(
              host->GetProcess(),
              bad_message::BadMessageReason::RFH_INVALID_WEB_UI_CONTROLLER);
          return;
        }

        const int size = sizeof...(WebUIControllerSubclasses);
        auto* contents = content::WebContents::FromRenderFrameHost(host);
        bool is_bound = BinderHelper<Interface, size - 1,
                                     std::tuple<WebUIControllerSubclasses...>>::
            BindInterface(contents->GetWebUI(), std::move(receiver));

        // This is expected to be called only for the right WebUI pages matching
        // the same WebUI associated to the RenderFrameHost.
        if (!is_bound) {
          ReceivedBadMessage(
              host->GetProcess(),
              bad_message::BadMessageReason::RFH_INVALID_WEB_UI_CONTROLLER);
          return;
        }
      }));
}

}  // namespace

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
void BindUnhandledTapWebContentsObserver(
    content::RenderFrameHost* const host,
    mojo::PendingReceiver<blink::mojom::UnhandledTapNotifier> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(host);
  if (!web_contents)
    return;

  auto* unhandled_tap_notifier_observer =
      contextual_search::UnhandledTapWebContentsObserver::FromWebContents(
          web_contents);
  if (!unhandled_tap_notifier_observer)
    return;

  contextual_search::CreateUnhandledTapNotifierImpl(
      unhandled_tap_notifier_observer->device_scale_factor(),
      unhandled_tap_notifier_observer->unhandled_tap_callback(),
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#if defined(OS_ANDROID)
void BindContextualSearchObserver(
    content::RenderFrameHost* const host,
    mojo::PendingReceiver<
        contextual_search::mojom::ContextualSearchJsApiService> receiver) {
  // Early return if the RenderFrameHost's delegate is not a WebContents.
  auto* web_contents = content::WebContents::FromRenderFrameHost(host);
  if (!web_contents)
    return;

  auto* contextual_search_observer =
      contextual_search::ContextualSearchObserver::FromWebContents(
          web_contents);
  if (contextual_search_observer) {
    contextual_search::CreateContextualSearchJsApiService(
        contextual_search_observer->api_handler(), std::move(receiver));
  }
}
#endif

// Forward image Annotator requests to the profile's AccessibilityLabelsService.
void BindImageAnnotator(
    content::RenderFrameHost* frame_host,
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
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  dom_distiller::DistillabilityDriver* driver =
      dom_distiller::DistillabilityDriver::FromWebContents(web_contents);
  if (!driver)
    return;
  driver->CreateDistillabilityService(std::move(receiver));
}

void BindDistillerJavaScriptService(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<dom_distiller::mojom::DistillerJavaScriptService>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  dom_distiller::DomDistillerService* dom_distiller_service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  auto* distiller_ui_handle = dom_distiller_service->GetDistillerUIHandle();
#if defined(OS_ANDROID)
  static_cast<dom_distiller::android::DistillerUIHandleAndroid*>(
      distiller_ui_handle)
      ->set_render_frame_host(frame_host);
#endif
  CreateDistillerJavaScriptService(distiller_ui_handle, std::move(receiver));
}

void BindPrerenderCanceler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mojom::PrerenderCanceler> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (!prerender_contents)
    return;
  prerender_contents->OnPrerenderCancelerReceiver(std::move(receiver));
}

void BindDocumentCoordinationUnit(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<performance_manager::mojom::DocumentCoordinationUnit>
        receiver) {
  auto* content = content::WebContents::FromRenderFrameHost(host);
  // |content| can be nullable if RenderFrameHost's delegate is not
  // WebContents.
  if (!content)
    return;
  auto* helper =
      performance_manager::PerformanceManagerTabHelper::FromWebContents(
          content);
  // This condition is for testing-only. We should handle a bind request after
  // PerformanceManagerTabHelper is attached to WebContents.
  if (!helper)
    return;
  return helper->BindDocumentCoordinationUnit(host, std::move(receiver));
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
void BindMimeHandlerService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<extensions::mime_handler::MimeHandlerService>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;
  extensions::MimeHandlerServiceImpl::Create(guest_view->GetStreamWeakPtr(),
                                             std::move(receiver));
}

void BindBeforeUnloadControl(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<extensions::mime_handler::BeforeUnloadControl>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;
  guest_view->FuseBeforeUnloadControl(std::move(receiver));
}
#endif

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  predictors::NetworkHintsHandlerImpl::Create(frame_host, std::move(receiver));
}

void PopulateChromeFrameBinders(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<image_annotation::mojom::Annotator>(
      base::BindRepeating(&BindImageAnnotator));

  map->Add<blink::mojom::AnchorElementMetricsHost>(
      base::BindRepeating(&NavigationPredictor::Create));

  map->Add<blink::mojom::InsecureInputService>(
      base::BindRepeating(&InsecureSensitiveInputDriverFactory::BindDriver));

  map->Add<dom_distiller::mojom::DistillabilityService>(
      base::BindRepeating(&BindDistillabilityService));

  map->Add<dom_distiller::mojom::DistillerJavaScriptService>(
      base::BindRepeating(&BindDistillerJavaScriptService));

  map->Add<mojom::PrerenderCanceler>(
      base::BindRepeating(&BindPrerenderCanceler));

  map->Add<performance_manager::mojom::DocumentCoordinationUnit>(
      base::BindRepeating(&BindDocumentCoordinationUnit));

  map->Add<translate::mojom::ContentTranslateDriver>(
      base::BindRepeating(&language::BindContentTranslateDriver));

#if defined(OS_ANDROID)
  map->Add<blink::mojom::InstalledAppProvider>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::InstalledAppProvider>));
#if defined(BROWSER_MEDIA_CONTROLS_MENU)
  map->Add<blink::mojom::MediaControlsMenuHost>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::MediaControlsMenuHost>));
#endif
  map->Add<chrome::mojom::OfflinePageAutoFetcher>(
      base::BindRepeating(&offline_pages::OfflinePageAutoFetcher::Create));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(base::BindRepeating(
        &ForwardToJavaFrame<payments::mojom::PaymentRequest>));
  }
  map->Add<blink::mojom::ShareService>(base::BindRepeating(
      &ForwardToJavaWebContents<blink::mojom::ShareService>));

  map->Add<contextual_search::mojom::ContextualSearchJsApiService>(
      base::BindRepeating(&BindContextualSearchObserver));

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
      base::BindRepeating(&badging::BadgeManager::BindFrameReceiver));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(
        base::BindRepeating(&payments::CreatePaymentRequest));
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  map->Add<extensions::mime_handler::MimeHandlerService>(
      base::BindRepeating(&BindMimeHandlerService));
  map->Add<extensions::mime_handler::BeforeUnloadControl>(
      base::BindRepeating(&BindBeforeUnloadControl));
#endif

  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));
}

void PopulateChromeWebUIFrameBinders(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map) {
  RegisterWebUIControllerInterfaceBinder<::mojom::BluetoothInternalsHandler,
                                         BluetoothInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::InterventionsInternalsPageHandler, InterventionsInternalsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      media::mojom::MediaEngagementScoreDetailsProvider, MediaEngagementUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      media_history::mojom::MediaHistoryStore, MediaHistoryUI>(map);

  RegisterWebUIControllerInterfaceBinder<::mojom::OmniboxPageHandler,
                                         OmniboxUI>(map);

  RegisterWebUIControllerInterfaceBinder<::mojom::SiteEngagementDetailsProvider,
                                         SiteEngagementUI>(map);

  RegisterWebUIControllerInterfaceBinder<::mojom::UsbInternalsPageHandler,
                                         UsbInternalsUI>(map);

#if defined(OS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<
      explore_sites_internals::mojom::PageHandler,
      explore_sites::ExploreSitesInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      snippets_internals::mojom::PageHandlerFactory, SnippetsInternalsUI>(map);
#else
  RegisterWebUIControllerInterfaceBinder<downloads::mojom::PageHandlerFactory,
                                         DownloadsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      new_tab_page::mojom::PageHandlerFactory, NewTabPageUI>(map);
#endif

#if defined(OS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<
      add_supervision::mojom::AddSupervisionHandler,
      chromeos::AddSupervisionUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      app_management::mojom::PageHandlerFactory,
      chromeos::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::cellular_setup::mojom::CellularSetup,
      chromeos::cellular_setup::CellularSetupDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::crostini_installer::mojom::PageHandlerFactory,
      chromeos::CrostiniInstallerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::crostini_upgrader::mojom::PageHandlerFactory,
      chromeos::CrostiniUpgraderUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::machine_learning::mojom::PageHandler,
      chromeos::machine_learning::MachineLearningInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::multidevice_setup::mojom::MultiDeviceSetup, chromeos::OobeUI,
      chromeos::multidevice::ProximityAuthUI,
      chromeos::multidevice_setup::MultiDeviceSetupDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::multidevice_setup::mojom::PrivilegedHostDeviceSetter,
      chromeos::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_config::mojom::CrosNetworkConfig,
      chromeos::InternetConfigDialogUI, chromeos::InternetDetailDialogUI,
      chromeos::NetworkUI, chromeos::OobeUI, chromeos::settings::OSSettingsUI>(
      map);
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<discards::mojom::DetailsProvider,
                                         DiscardsUI>(map);

  RegisterWebUIControllerInterfaceBinder<discards::mojom::GraphDump,
                                         DiscardsUI>(map);
#endif

#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
  RegisterWebUIControllerInterfaceBinder<feed_internals::mojom::PageHandler,
                                         FeedInternalsUI>(map);
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  RegisterWebUIControllerInterfaceBinder<::mojom::ResetPasswordHandler,
                                         ResetPasswordUI>(map);
#endif

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
  RegisterWebUIControllerInterfaceBinder<media::mojom::KaleidoscopeDataProvider,
                                         KaleidoscopeUI>(map);
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)
}

}  // namespace internal
}  // namespace chrome
