// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/bookmark_app_extension_util.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"
#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/origin_trials/chrome_origin_trial_policy.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_CHROMEOS)
#include "base/strings/string_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#endif

namespace extensions {

namespace {

// Class to handle installing a bookmark app after it has synced. Handles
// downloading and decoding the icons.
class BookmarkAppInstaller : public base::RefCounted<BookmarkAppInstaller>,
                             public content::WebContentsObserver {
 public:
  BookmarkAppInstaller(extensions::ExtensionService* service,
                       const WebApplicationInfo& web_app_info,
                       bool is_locally_installed)
      : service_(service),
        web_app_info_(web_app_info),
        is_locally_installed_(is_locally_installed) {}

  void Run() {
    for (const auto& icon : web_app_info_.icons) {
      if (icon.url.is_valid())
        urls_to_download_.push_back(icon.url);
    }

    if (!urls_to_download_.empty()) {
      // Matched in OnIconsDownloaded.
      AddRef();
      SetupWebContents();

      return;
    }

    DoInstallation();
  }

  void SetupWebContents() {
    // Spin up a web contents process so we can use WebAppIconDownloader.
    // This is necessary to make sure we pick up all of the images provided
    // in favicon URLs. Without this, bookmark app sync can fail due to
    // missing icons which are not correctly extracted from a favicon.
    // (The eventual error indicates that there are missing files, which
    // are the not-extracted favicon images).
    //
    // TODO(dominickn): refactor bookmark app syncing to reuse one web
    // contents for all pending synced bookmark apps. This will avoid
    // pathological cases where n renderers for n bookmark apps are spun up on
    // first sign-in to a new machine.
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(service_->profile()));
    Observe(web_contents_.get());

    // Load about:blank so that the process actually starts.
    // Image download continues in DidFinishLoad.
    content::NavigationController::LoadURLParams load_params(
        GURL("about:blank"));
    load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
    web_contents_->GetController().LoadURLWithParams(load_params);
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    web_app_icon_downloader_.reset(new web_app::WebAppIconDownloader(
        web_contents_.get(), urls_to_download_,
        "Extensions.BookmarkApp.Icon.HttpStatusCodeClassOnSync",
        base::BindOnce(&BookmarkAppInstaller::OnIconsDownloaded,
                       base::Unretained(this))));

    // Skip downloading the page favicons as everything in is the URL list.
    web_app_icon_downloader_->SkipPageFavicons();
    web_app_icon_downloader_->Start();
  }

 private:
  friend class base::RefCounted<BookmarkAppInstaller>;
  ~BookmarkAppInstaller() override {}

  void OnIconsDownloaded(bool success,
                         const std::map<GURL, std::vector<SkBitmap>>& bitmaps) {
    // Ignore the unsuccessful case, as the necessary icons will be generated.
    if (success) {
      for (const auto& url_bitmaps : bitmaps) {
        for (const auto& bitmap : url_bitmaps.second) {
          // Only accept square icons.
          if (bitmap.empty() || bitmap.width() != bitmap.height())
            continue;

          downloaded_bitmaps_.push_back(
              web_app::BitmapAndSource(url_bitmaps.first, bitmap));
        }
      }
    }
    DoInstallation();
    Release();
  }

  void DoInstallation() {
    // Ensure that all icons that are in web_app_info are present, by generating
    // icons for any sizes which have failed to download. This ensures that the
    // created manifest for the bookmark app does not contain links to icons
    // which are not actually created and linked on disk.

    // Ensure that all icon widths in the web app info icon array are present in
    // the sizes to generate set. This ensures that we will have all of the
    // icon sizes from when the app was originally added, even if icon URLs are
    // no longer accessible.
    std::set<int> sizes_to_generate = web_app::SizesToGenerate();
    for (const auto& icon : web_app_info_.icons)
      sizes_to_generate.insert(icon.width);

    web_app_info_.generated_icon_color = SK_ColorTRANSPARENT;
    std::map<int, web_app::BitmapAndSource> size_map =
        web_app::ResizeIconsAndGenerateMissing(
            downloaded_bitmaps_, sizes_to_generate, web_app_info_.app_url,
            &web_app_info_.generated_icon_color);

    web_app::UpdateWebAppIconsWithoutChangingLinks(size_map, &web_app_info_);

    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service_));
    installer->set_error_on_unsupported_requirements(true);
    installer->set_installer_callback(base::BindOnce(
        &BookmarkAppInstaller::OnInstallationDone, this, installer));
    installer->InstallWebApp(web_app_info_);
  }

  void OnInstallationDone(scoped_refptr<CrxInstaller> installer,
                          const base::Optional<CrxInstallError>& result) {
    // No result means success.
    if (!result.has_value()) {
      SetBookmarkAppIsLocallyInstalled(service_->GetBrowserContext(),
                                       installer->extension(),
                                       is_locally_installed_);
    }
  }

  extensions::ExtensionService* service_;
  WebApplicationInfo web_app_info_;
  bool is_locally_installed_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<web_app::WebAppIconDownloader> web_app_icon_downloader_;
  std::vector<GURL> urls_to_download_;
  std::vector<web_app::BitmapAndSource> downloaded_bitmaps_;
};

#if defined(OS_CHROMEOS)
const char kChromeOsPlayPlatform[] = "chromeos_play";
const char kPlayIntentPrefix[] =
    "https://play.google.com/store/apps/details?id=";

std::string ExtractQueryValueForName(const GURL& url, const std::string& name) {
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == name)
      return it.GetValue();
  }
  return std::string();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

BookmarkAppHelper::BookmarkAppHelper(
    Profile* profile,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    content::WebContents* contents,
    WebappInstallSource install_source)
    : profile_(profile),
      contents_(contents),
      web_app_info_(*web_app_info),
      crx_installer_(CrxInstaller::CreateSilent(
          ExtensionSystem::Get(profile)->extension_service())),
      for_installable_site_(web_app::ForInstallableSite::kUnknown),
      install_source_(install_source) {
  if (contents)
    installable_manager_ = InstallableManager::FromWebContents(contents);

  // The default app title is the page title, which can be quite long. Limit the
  // default name used to something sensible.
  const int kMaxDefaultTitle = 40;
  if (web_app_info_.title.length() > kMaxDefaultTitle) {
    web_app_info_.title = web_app_info_.title.substr(0, kMaxDefaultTitle - 3) +
                          base::UTF8ToUTF16("...");
  }

  registrar_.Add(this, NOTIFICATION_CRX_INSTALLER_DONE,
                 content::Source<CrxInstaller>(crx_installer_.get()));

  registrar_.Add(this, NOTIFICATION_EXTENSION_INSTALL_ERROR,
                 content::Source<CrxInstaller>(crx_installer_.get()));

  crx_installer_->set_error_on_unsupported_requirements(true);
}

BookmarkAppHelper::~BookmarkAppHelper() {}

void BookmarkAppHelper::Create(const CreateBookmarkAppCallback& callback) {
  callback_ = callback;

  if (is_no_network_install_) {
    // |for_installable_site_| is determined by fetching a manifest and running
    // the eligibility check on it. If we don't hit the network, assume that the
    // app represetned by |web_app_info_| is installable.
    for_installable_site_ = web_app::ForInstallableSite::kYes;
    OnIconsDownloaded(true, std::map<GURL, std::vector<SkBitmap>>());
    // Do not fetch the manifest for extension URLs.
  } else if (contents_ &&
             !contents_->GetVisibleURL().SchemeIs(kExtensionScheme)) {
    // Null in tests. OnDidPerformInstallableCheck is called via a testing API.
    // TODO(crbug.com/829232) ensure this is consistent with other calls to
    // GetData.
    if (installable_manager_) {
      InstallableParams params;
      params.check_eligibility = true;
      params.valid_primary_icon = true;
      params.valid_manifest = true;
      params.check_webapp_manifest_display = false;
      // Do not wait for a service worker if it doesn't exist.
      params.has_worker = !bypass_service_worker_check_;
      installable_manager_->GetData(
          params,
          base::BindOnce(&BookmarkAppHelper::OnDidPerformInstallableCheck,
                         weak_factory_.GetWeakPtr()));
    }
  } else {
    for_installable_site_ = web_app::ForInstallableSite::kNo;
    OnIconsDownloaded(true, std::map<GURL, std::vector<SkBitmap>>());
  }
}

void BookmarkAppHelper::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  DCHECK(data.manifest_url.is_valid() || data.manifest->IsEmpty());

  if (contents_->IsBeingDestroyed())
    return;

  if (require_manifest_ && !data.valid_manifest) {
    LOG(WARNING) << "Did not install " << web_app_info_.app_url.spec()
                 << " because it didn't have a manifest";
    callback_.Run(nullptr, web_app_info_);
    return;
  }

  for_installable_site_ = data.errors.empty() && !shortcut_app_requested_
                              ? web_app::ForInstallableSite::kYes
                              : web_app::ForInstallableSite::kNo;

  web_app::UpdateWebAppInfoFromManifest(*data.manifest, &web_app_info_,
                                        for_installable_site_);

  const std::vector<GURL> web_app_info_icon_urls =
      web_app::GetValidIconUrlsToDownload(web_app_info_, &data);

  web_app::MergeInstallableDataIcon(data, &web_app_info_);

  web_app_icon_downloader_.reset(new web_app::WebAppIconDownloader(
      contents_, web_app_info_icon_urls,
      "Extensions.BookmarkApp.Icon.HttpStatusCodeClassOnCreate",
      base::BindOnce(&BookmarkAppHelper::OnIconsDownloaded,
                     weak_factory_.GetWeakPtr())));

  // If the manifest specified icons, or this is a System App, don't use the
  // page icons.
  if (!data.manifest->icons.empty() ||
      contents_->GetVisibleURL().SchemeIs(content::kChromeUIScheme))
    web_app_icon_downloader_->SkipPageFavicons();

  // If we tried to check for an intent to the Play Store, wait for the async
  // reply.
  if (DidCheckForIntentToPlayStore(*data.manifest))
    return;

  web_app_icon_downloader_->Start();
}

void BookmarkAppHelper::OnIconsDownloaded(
    bool success,
    const std::map<GURL, std::vector<SkBitmap>>& bitmaps) {
  // The tab has navigated away during the icon download. Cancel the bookmark
  // app creation.
  if (!success) {
    web_app_icon_downloader_.reset();
    callback_.Run(nullptr, web_app_info_);
    return;
  }

  std::vector<web_app::BitmapAndSource> downloaded_icons =
      web_app::FilterSquareIcons(bitmaps, web_app_info_);
  web_app::ResizeDownloadedIconsGenerateMissing(std::move(downloaded_icons),
                                                &web_app_info_);

  web_app_icon_downloader_.reset();

  if (!contents_) {
    // The web contents can be null in tests.
    OnBubbleCompleted(true,
                      std::make_unique<WebApplicationInfo>(web_app_info_));
    return;
  }

  Browser* browser = chrome::FindBrowserWithWebContents(contents_);
  if (!browser) {
    // The browser can be null in tests.
    OnBubbleCompleted(true,
                      std::make_unique<WebApplicationInfo>(web_app_info_));
    return;
  }

  // TODO(alancutter): Get user confirmation before entering the install flow,
  // installation code shouldn't have to perform UI work.
  if (for_installable_site_ == web_app::ForInstallableSite::kYes) {
    web_app_info_.open_as_window = true;
      chrome::ShowPWAInstallBubble(
          contents_, std::make_unique<WebApplicationInfo>(web_app_info_),
          base::BindOnce(&BookmarkAppHelper::OnBubbleCompleted,
                         weak_factory_.GetWeakPtr()));
  } else {
    chrome::ShowBookmarkAppDialog(
        contents_, std::make_unique<WebApplicationInfo>(web_app_info_),
        base::BindOnce(&BookmarkAppHelper::OnBubbleCompleted,
                       weak_factory_.GetWeakPtr()));
  }
}

void BookmarkAppHelper::OnBubbleCompleted(
    bool user_accepted,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  if (user_accepted) {
    DCHECK(web_app_info);
    web_app_info_ = *web_app_info;

    if (is_policy_installed_app_)
      crx_installer_->set_install_source(Manifest::EXTERNAL_POLICY_DOWNLOAD);

    if (is_default_app_) {
      crx_installer_->set_install_source(Manifest::EXTERNAL_PREF_DOWNLOAD);
      // InstallWebApp will OR the creation flags with FROM_BOOKMARK.
      crx_installer_->set_creation_flags(Extension::WAS_INSTALLED_BY_DEFAULT);
    }

    if (is_system_app_) {
      // System Apps are considered EXTERNAL_COMPONENT as they are downloaded
      // from the WebUI they point to. COMPONENT seems like the more correct
      // value, but usages (icon loading, filesystem cleanup), are tightly
      // coupled to this value, making it unsuitable.
      crx_installer_->set_install_source(Manifest::EXTERNAL_COMPONENT);
      // InstallWebApp will OR the creation flags with FROM_BOOKMARK.
      crx_installer_->set_creation_flags(Extension::WAS_INSTALLED_BY_DEFAULT);
    }

    if (is_no_network_install_) {
      // Ensure that this app is not synced. A no-network install means we have
      // all data locally, so assume that there is some mechanism to propagate
      // the local source of data in place of usual extension sync.
      crx_installer_->set_install_source(Manifest::EXTERNAL_PREF_DOWNLOAD);
    }

    crx_installer_->InstallWebApp(web_app_info_);

    if (InstallableMetrics::IsReportableInstallSource(install_source_) &&
        for_installable_site_ == web_app::ForInstallableSite::kYes) {
      InstallableMetrics::TrackInstallEvent(install_source_);
    }
  } else {
    callback_.Run(nullptr, web_app_info_);
  }
}

void BookmarkAppHelper::FinishInstallation(const Extension* extension) {
  // Set the default 'open as' preference for use next time the dialog is
  // shown.
  LaunchType launch_type =
      web_app_info_.open_as_window ? LAUNCH_TYPE_WINDOW : LAUNCH_TYPE_REGULAR;

  if (forced_launch_type_)
    launch_type = forced_launch_type_.value();

  // Set the launcher type for the app.
  SetLaunchType(profile_, extension->id(), launch_type);

  // Set this app to be locally installed, as it was installed from this
  // machine.
  SetBookmarkAppIsLocallyInstalled(profile_, extension, true);

  if (!contents_) {
    // The web contents can be null in tests.
    callback_.Run(extension, web_app_info_);
    return;
  }

  web_app::RecordAppBanner(contents_, web_app_info_.app_url);
  web_app::RecordWebAppInstallationTimestamp(profile_->GetPrefs(),
                                             extension->id(), install_source_);

  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  if (add_to_applications_menu_ && CanBookmarkAppCreateOsShortcuts()) {
    BookmarkAppCreateOsShortcuts(
        profile_, extension, add_to_desktop_,
        base::BindOnce(&BookmarkAppHelper::OnShortcutCreationCompleted,
                       weak_factory_.GetWeakPtr(), extension->id()));
  } else {
    OnShortcutCreationCompleted(extension->id(), false /* shortcuts_created */);
  }
}

void BookmarkAppHelper::OnShortcutCreationCompleted(
    const std::string& extension_id,
    bool shortcut_created) {
  // Note that the extension may have been deleted since this task was posted,
  // so use |extension_id| as a weak pointer.
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
          extension_id);
  if (!extension) {
    callback_.Run(nullptr, web_app_info_);
    return;
  }

  if (add_to_quick_launch_bar_ && CanBookmarkAppBePinnedToShelf())
    BookmarkAppPinToShelf(extension);

  // If there is a browser, it means that the app is being installed in the
  // foreground.
  const bool reparent_tab =
      (chrome::FindBrowserWithWebContents(contents_) != nullptr);

  // TODO(loyso): Reparenting must be implemented in
  // chrome/browser/ui/web_applications/ UI layer as a post-install step.
  if (reparent_tab &&
      CanBookmarkAppReparentTab(profile_, extension, shortcut_created)) {
    DCHECK(!profile_->IsOffTheRecord());
    BookmarkAppReparentTab(contents_, extension);
    if (CanBookmarkAppRevealAppShim())
      BookmarkAppRevealAppShim(profile_, extension);
  }

  callback_.Run(extension, web_app_info_);
}

bool BookmarkAppHelper::DidCheckForIntentToPlayStore(
    const blink::Manifest& manifest) {
#if defined(OS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(features::kApkWebAppInstalls))
    return false;

  if (for_installable_site_ != web_app::ForInstallableSite::kYes)
    return false;

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

    // Attach the referrer value.
    std::string referrer =
        ExtractQueryValueForName(application.url, "referrer");
    if (!referrer.empty())
      referrer = "&referrer=" + referrer;

    std::string intent = kPlayIntentPrefix + id + referrer;

    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->app(), IsInstallable);
      if (instance) {
        instance->IsInstallable(
            id,
            base::BindOnce(&BookmarkAppHelper::OnDidCheckForIntentToPlayStore,
                           weak_factory_.GetWeakPtr(), intent));
        return true;
      }
    }
  }
#endif  // defined(OS_CHROMEOS)
  return false;
}

void BookmarkAppHelper::OnDidCheckForIntentToPlayStore(
    const std::string& intent,
    bool should_intent_to_store) {
#if defined(OS_CHROMEOS)
  if (should_intent_to_store) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrl);
      if (instance) {
        instance->HandleUrl(intent, arc::kPlayStorePackage);
        callback_.Run(nullptr, web_app_info_);
        return;
      }
    }
  }
#endif  // defined(OS_CHROMEOS)

  web_app_icon_downloader_->Start();
}

void BookmarkAppHelper::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  // TODO(dominickn): bookmark app creation fails when extensions cannot be
  // created (e.g. due to management policies). Add to shelf visibility should
  // be gated on whether extensions can be created - see crbug.com/545541.
  switch (type) {
    case NOTIFICATION_CRX_INSTALLER_DONE: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension) {
        DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension),
                  web_app_info_.app_url);
        FinishInstallation(extension);
      } else {
        callback_.Run(nullptr, web_app_info_);
      }
      break;
    }
    case NOTIFICATION_EXTENSION_INSTALL_ERROR:
      callback_.Run(nullptr, web_app_info_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CreateOrUpdateBookmarkApp(extensions::ExtensionService* service,
                               WebApplicationInfo* web_app_info,
                               bool is_locally_installed) {
  scoped_refptr<BookmarkAppInstaller> installer(
      new BookmarkAppInstaller(service, *web_app_info, is_locally_installed));
  installer->Run();
}

}  // namespace extensions
