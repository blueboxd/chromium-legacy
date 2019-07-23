// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_task.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "net/base/url_util.h"
#endif

namespace web_app {

namespace {

#if defined(OS_CHROMEOS)
const char kChromeOsPlayPlatform[] = "chromeos_play";
const char kPlayIntentPrefix[] =
    "https://play.google.com/store/apps/details?id=";
const char kPlayStorePackage[] = "com.android.vending";

std::string ExtractQueryValueForName(const GURL& url, const std::string& name) {
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == name)
      return it.GetValue();
  }
  return std::string();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

WebAppInstallTask::WebAppInstallTask(
    Profile* profile,
    InstallFinalizer* install_finalizer,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : data_retriever_(std::move(data_retriever)),
      install_finalizer_(install_finalizer),
      profile_(profile) {}

WebAppInstallTask::~WebAppInstallTask() = default;

void WebAppInstallTask::InstallWebAppFromManifest(
    content::WebContents* contents,
    WebappInstallSource install_source,
    InstallManager::WebAppInstallDialogCallback dialog_callback,
    InstallManager::OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  auto web_app_info = std::make_unique<WebApplicationInfo>();

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     base::Unretained(this), std::move(web_app_info),
                     /*force_shortcut_app=*/false));
}

void WebAppInstallTask::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    InstallManager::WebAppInstallDialogCallback dialog_callback,
    InstallManager::OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebApplicationInfo,
                     base::Unretained(this), force_shortcut_app));
}

void WebAppInstallTask::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool no_network_install,
    WebappInstallSource install_source,
    InstallManager::OnceInstallCallback callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  std::vector<BitmapAndSource> square_icons;
  FilterSquareIconsFromInfo(*web_application_info, &square_icons);
  ResizeDownloadedIconsGenerateMissing(std::move(square_icons),
                                       web_application_info.get());

  install_source_ = install_source;
  background_installation_ = true;

  // |for_installable_site| arg is determined by fetching a manifest and running
  // the eligibility check on it. If we don't hit the network, assume that the
  // app represetned by |web_application_info| is installable.
  RecordInstallEvent(no_network_install ? ForInstallableSite::kYes
                                        : ForInstallableSite::kNo);

  InstallFinalizer::FinalizeOptions options;
  options.install_source = install_source;
  if (no_network_install) {
    options.no_network_install = true;
    // We should only install windowed apps via this method.
    options.force_launch_container = LaunchContainer::kWindow;
  }

  install_finalizer_->FinalizeInstall(*web_application_info, options,
                                      std::move(callback));
}

void WebAppInstallTask::InstallWebAppWithOptions(
    content::WebContents* contents,
    const ExternalInstallOptions& install_options,
    InstallManager::OnceInstallCallback install_callback) {
  CheckInstallPreconditions();

  Observe(contents);
  install_callback_ = std::move(install_callback);
  install_source_ = ConvertExternalInstallSourceToInstallSource(
      install_options.install_source);
  install_options_ = install_options;
  background_installation_ = true;

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebApplicationInfo,
                     base::Unretained(this), /*force_shortcut_app=*/false));
}

void WebAppInstallTask::InstallWebAppFromInfoRetrieveIcons(
    content::WebContents* web_contents,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool is_locally_installed,
    WebappInstallSource install_source,
    InstallManager::OnceInstallCallback callback) {
  CheckInstallPreconditions();

  Observe(web_contents);
  install_callback_ = std::move(callback);
  install_source_ = install_source;
  background_installation_ = true;

  std::vector<GURL> icon_urls =
      GetValidIconUrlsToDownload(*web_application_info, /*data=*/nullptr);

  // Skip downloading the page favicons as everything in is the URL list.
  data_retriever_->GetIcons(
      web_contents, icon_urls, /*skip_page_fav_icons*/ true, install_source_,
      base::BindOnce(&WebAppInstallTask::OnIconsRetrieved,
                     base::Unretained(this), std::move(web_application_info),
                     is_locally_installed));
}

void WebAppInstallTask::WebContentsDestroyed() {
  CallInstallCallback(AppId(), InstallResultCode::kWebContentsDestroyed);
}

void WebAppInstallTask::SetInstallFinalizerForTesting(
    InstallFinalizer* install_finalizer) {
  install_finalizer_ = install_finalizer;
}

void WebAppInstallTask::CheckInstallPreconditions() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Concurrent calls are not allowed.
  DCHECK(!web_contents());
  CHECK(!install_callback_);
}

void WebAppInstallTask::RecordInstallEvent(
    ForInstallableSite for_installable_site) {
  DCHECK(install_source_ != kNoInstallSource);

  if (InstallableMetrics::IsReportableInstallSource(install_source_) &&
      for_installable_site == ForInstallableSite::kYes) {
    InstallableMetrics::TrackInstallEvent(install_source_);
  }
}

void WebAppInstallTask::CallInstallCallback(const AppId& app_id,
                                            InstallResultCode code) {
  Observe(nullptr);
  dialog_callback_.Reset();

  DCHECK(install_source_ != kNoInstallSource);
  install_source_ = kNoInstallSource;

  DCHECK(install_callback_);
  std::move(install_callback_).Run(app_id, code);
}

bool WebAppInstallTask::ShouldStopInstall() const {
  // Install should stop early if WebContents is being destroyed.
  // WebAppInstallTask::WebContentsDestroyed will get called eventually and the
  // callback will be invoked at that point.
  return !web_contents() || web_contents()->IsBeingDestroyed();
}

void WebAppInstallTask::OnGetWebApplicationInfo(
    bool force_shortcut_app,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (ShouldStopInstall())
    return;

  if (!web_app_info) {
    CallInstallCallback(AppId(),
                        InstallResultCode::kGetWebApplicationInfoFailed);
    return;
  }

  bool bypass_service_worker_check = false;
  if (install_options_)
    bypass_service_worker_check = install_options_->bypass_service_worker_check;

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(), bypass_service_worker_check,
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     base::Unretained(this), std::move(web_app_info),
                     force_shortcut_app));
}

void WebAppInstallTask::OnDidPerformInstallableCheck(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    bool force_shortcut_app,
    const blink::Manifest& manifest,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  if (install_options_ && install_options_->require_manifest &&
      !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << web_app_info->app_url.spec()
                 << " because it didn't have a manifest for web app";
    CallInstallCallback(AppId(), InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  const auto for_installable_site = is_installable && !force_shortcut_app
                                        ? ForInstallableSite::kYes
                                        : ForInstallableSite::kNo;

  UpdateWebAppInfoFromManifest(manifest, web_app_info.get(),
                               for_installable_site);

  std::vector<GURL> icon_urls =
      GetValidIconUrlsToDownload(*web_app_info, /*data=*/nullptr);

  // A system app should always have a manifest icon.
  if (install_options_ && install_options_->install_source ==
                              ExternalInstallSource::kSystemInstalled) {
    DCHECK(!manifest.icons.empty());
  }

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = !manifest.icons.empty();

  CheckForPlayStoreIntentOrGetIcons(manifest, std::move(web_app_info),
                                    std::move(icon_urls), for_installable_site,
                                    skip_page_favicons);
}

void WebAppInstallTask::CheckForPlayStoreIntentOrGetIcons(
    const blink::Manifest& manifest,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    std::vector<GURL> icon_urls,
    ForInstallableSite for_installable_site,
    bool skip_page_favicons) {
#if defined(OS_CHROMEOS)
  // If we have install options, this is not a user-triggered install, and thus
  // cannot be sent to the store.
  if (base::FeatureList::IsEnabled(features::kApkWebAppInstalls) &&
      for_installable_site == ForInstallableSite::kYes && !install_options_) {
    for (const auto& application : manifest.related_applications) {
      std::string id = base::UTF16ToUTF8(application.id.string());
      if (!base::EqualsASCII(application.platform.string(),
                             kChromeOsPlayPlatform)) {
        continue;
      }

      std::string id_from_app_url =
          ExtractQueryValueForName(application.url, "id");

      if (id.empty()) {
        if (id_from_app_url.empty())
          continue;
        id = id_from_app_url;
      }

      auto* arc_service_manager = arc::ArcServiceManager::Get();
      if (arc_service_manager) {
        auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
            arc_service_manager->arc_bridge_service()->app(), IsInstallable);
        if (instance) {
          // Attach the referrer value.
          std::string referrer =
              ExtractQueryValueForName(application.url, "referrer");
          if (!referrer.empty())
            referrer = "&referrer=" + referrer;

          std::string intent = kPlayIntentPrefix + id + referrer;
          instance->IsInstallable(
              id,
              base::BindOnce(&WebAppInstallTask::OnDidCheckForIntentToPlayStore,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(web_app_info), std::move(icon_urls),
                             for_installable_site, skip_page_favicons, intent));
          return;
        }
      }
    }
  }

#endif  // defined(OS_CHROMEOS)
  OnDidCheckForIntentToPlayStore(std::move(web_app_info), std::move(icon_urls),
                                 for_installable_site, skip_page_favicons,
                                 /*intent=*/"",
                                 /*should_intent_to_store=*/false);
}

void WebAppInstallTask::OnDidCheckForIntentToPlayStore(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    std::vector<GURL> icon_urls,
    ForInstallableSite for_installable_site,
    bool skip_page_favicons,
    const std::string& intent,
    bool should_intent_to_store) {
#if defined(OS_CHROMEOS)
  if (should_intent_to_store && !intent.empty()) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrl);
      if (instance) {
        instance->HandleUrl(intent, kPlayStorePackage);
        CallInstallCallback(AppId(), InstallResultCode::kIntentToPlayStore);
        return;
      }
    }
  }
#endif  // defined(OS_CHROMEOS)

  data_retriever_->GetIcons(
      web_contents(), icon_urls, skip_page_favicons, install_source_,
      base::BindOnce(&WebAppInstallTask::OnIconsRetrievedShowDialog,
                     base::Unretained(this), std::move(web_app_info),
                     for_installable_site));
}

void WebAppInstallTask::OnIconsRetrieved(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    bool is_locally_installed,
    IconsMap icons_map) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  std::vector<BitmapAndSource> downloaded_icons;
  FilterSquareIconsFromMap(icons_map, &downloaded_icons);

  // Ensure that all icons that are in web_app_info are present, by generating
  // icons for any sizes which have failed to download. This ensures that the
  // created manifest for the web app does not contain links to icons
  // which are not actually created and linked on disk.

  // Ensure that all icon widths in the web app info icon array are present in
  // the sizes to generate set. This ensures that we will have all of the
  // icon sizes from when the app was originally added, even if icon URLs are
  // no longer accessible.
  std::set<int> sizes_to_generate = SizesToGenerate();
  for (const auto& icon : web_app_info->icons)
    sizes_to_generate.insert(icon.width);

  web_app_info->generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_map = ResizeIconsAndGenerateMissing(
      downloaded_icons, sizes_to_generate, web_app_info->app_url,
      &web_app_info->generated_icon_color);

  UpdateWebAppIconsWithoutChangingLinks(size_map, web_app_info.get());

  InstallFinalizer::FinalizeOptions options;
  options.install_source = install_source_;
  options.locally_installed = is_locally_installed;

  install_finalizer_->FinalizeInstall(
      *web_app_info, options,
      base::BindOnce(&WebAppInstallTask::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppInstallTask::OnIconsRetrievedShowDialog(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    IconsMap icons_map) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  std::vector<BitmapAndSource> downloaded_icons =
      FilterSquareIcons(icons_map, *web_app_info);
  ResizeDownloadedIconsGenerateMissing(std::move(downloaded_icons),
                                       web_app_info.get());

  if (background_installation_) {
    DCHECK(!dialog_callback_);
    OnDialogCompleted(for_installable_site, /*user_accepted=*/true,
                      std::move(web_app_info));
  } else {
    DCHECK(dialog_callback_);
    std::move(dialog_callback_)
        .Run(web_contents(), std::move(web_app_info), for_installable_site,
             base::BindOnce(&WebAppInstallTask::OnDialogCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            for_installable_site));
  }
}

void WebAppInstallTask::OnDialogCompleted(
    ForInstallableSite for_installable_site,
    bool user_accepted,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  if (ShouldStopInstall())
    return;

  if (!user_accepted) {
    CallInstallCallback(AppId(), InstallResultCode::kUserInstallDeclined);
    return;
  }

  WebApplicationInfo web_app_info_copy = *web_app_info;

  // This metric is recorded regardless of the installation result.
  RecordInstallEvent(for_installable_site);

  InstallFinalizer::FinalizeOptions finalize_options;
  finalize_options.install_source = install_source_;
  if (install_options_) {
    finalize_options.force_launch_container =
        install_options_->launch_container;
  }

  install_finalizer_->FinalizeInstall(
      web_app_info_copy, finalize_options,
      base::BindOnce(&WebAppInstallTask::OnInstallFinalizedCreateShortcuts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info)));

  // Check that the finalizer hasn't called OnInstallFinalizedCreateShortcuts
  // synchronously:
  DCHECK(install_callback_);
}

void WebAppInstallTask::OnInstallFinalized(const AppId& app_id,
                                           InstallResultCode code) {
  if (ShouldStopInstall())
    return;

  CallInstallCallback(app_id, code);
}

void WebAppInstallTask::OnInstallFinalizedCreateShortcuts(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    const AppId& app_id,
    InstallResultCode code) {
  if (ShouldStopInstall())
    return;

  if (code != InstallResultCode::kSuccess) {
    CallInstallCallback(app_id, code);
    return;
  }

  RecordAppBanner(web_contents(), web_app_info->app_url);
  RecordWebAppInstallationTimestamp(profile_->GetPrefs(), app_id,
                                    install_source_);

  bool add_to_applications_menu = true;
  bool add_to_desktop = true;

  if (install_options_) {
    add_to_applications_menu = install_options_->add_to_applications_menu;
    add_to_desktop = install_options_->add_to_desktop;
  }

  auto create_shortcuts_callback = base::BindOnce(
      &WebAppInstallTask::OnShortcutsCreated, weak_ptr_factory_.GetWeakPtr(),
      std::move(web_app_info), app_id);

  if (add_to_applications_menu && install_finalizer_->CanCreateOsShortcuts()) {
    // TODO(ortuno): Make adding a shortcut to the applications menu independent
    // from adding a shortcut to desktop.
    install_finalizer_->CreateOsShortcuts(app_id, add_to_desktop,
                                          std::move(create_shortcuts_callback));
  } else {
    std::move(create_shortcuts_callback).Run(false /* created_shortcuts */);
  }
}

void WebAppInstallTask::OnShortcutsCreated(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    const AppId& app_id,
    bool shortcut_created) {
  if (ShouldStopInstall())
    return;

  bool add_to_quick_launch_bar = true;
  if (install_options_)
    add_to_quick_launch_bar = install_options_->add_to_quick_launch_bar;

  if (add_to_quick_launch_bar && install_finalizer_->CanPinAppToShelf())
    install_finalizer_->PinAppToShelf(app_id);

  // TODO(loyso): Reparenting must be implemented in
  // chrome/browser/ui/web_applications/ UI layer as a post-install step.
  if (!background_installation_) {
    const bool can_reparent_tab =
        install_finalizer_->CanReparentTab(app_id, shortcut_created);
    if (can_reparent_tab && web_app_info->open_as_window)
      install_finalizer_->ReparentTab(app_id, web_contents());

    // TODO(loyso): Make revealing app shim independent from CanReparentTab.
    if (can_reparent_tab && install_finalizer_->CanRevealAppShim())
      install_finalizer_->RevealAppShim(app_id);
  }

  CallInstallCallback(app_id, InstallResultCode::kSuccess);
}

}  // namespace web_app
