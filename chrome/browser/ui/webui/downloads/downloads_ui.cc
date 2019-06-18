// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/ui/webui/downloads/downloads_dom_handler.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/downloads_resources.h"
#include "chrome/grit/downloads_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/webui_resources.h"

using content::BrowserContext;
using content::DownloadManager;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateDownloadsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDownloadsHost);

  bool requests_ap_verdicts = safe_browsing::AdvancedProtectionStatusManager::
      RequestsAdvancedProtectionVerdicts(profile);
  source->AddBoolean("requestsApVerdicts", requests_ap_verdicts);

  static constexpr LocalizedString kStrings[] = {
      {"title", IDS_DOWNLOAD_TITLE},
      {"searchResultsPlural", IDS_SEARCH_RESULTS_PLURAL},
      {"searchResultsSingular", IDS_SEARCH_RESULTS_SINGULAR},
      {"downloads", IDS_DOWNLOAD_TITLE},
      {"actionMenuDescription", IDS_DOWNLOAD_ACTION_MENU_DESCRIPTION},
      {"clearAll", IDS_DOWNLOAD_LINK_CLEAR_ALL},
      {"clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH},
      {"openDownloadsFolder", IDS_DOWNLOAD_LINK_OPEN_DOWNLOADS_FOLDER},
      {"moreActions", IDS_DOWNLOAD_MORE_ACTIONS},
      {"search", IDS_DOWNLOAD_SEARCH},

      // No results message that shows instead of the downloads list.
      {"noDownloads", IDS_DOWNLOAD_NO_DOWNLOADS},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},

      // Status.
      {"statusCancelled", IDS_DOWNLOAD_TAB_CANCELLED},
      {"statusRemoved", IDS_DOWNLOAD_FILE_REMOVED},

      // Dangerous file.
      {"dangerFileDesc", IDS_BLOCK_REASON_GENERIC_DOWNLOAD},
      {"dangerSave", IDS_CONFIRM_DOWNLOAD},
      {"dangerRestore", IDS_CONFIRM_DOWNLOAD_RESTORE},
      {"dangerDiscard", IDS_DISCARD_DOWNLOAD},

      // Controls.
      {"controlPause", IDS_DOWNLOAD_LINK_PAUSE},
      {"controlCancel", IDS_DOWNLOAD_LINK_CANCEL},
      {"controlResume", IDS_DOWNLOAD_LINK_RESUME},
      {"controlRemoveFromList", IDS_DOWNLOAD_LINK_REMOVE},
      {"controlRemoveFromListAriaLabel", IDS_DOWNLOAD_LINK_REMOVE_ARIA_LABEL},
      {"controlRetry", IDS_DOWNLOAD_LINK_RETRY},
      {"controlledByUrl", IDS_DOWNLOAD_BY_EXTENSION_URL},
      {"toastClearedAll", IDS_DOWNLOAD_TOAST_CLEARED_ALL},
      {"toastRemovedFromList", IDS_DOWNLOAD_TOAST_REMOVED_FROM_LIST},
      {"undo", IDS_DOWNLOAD_UNDO},
  };
  AddLocalizedStringsBulk(source, kStrings, base::size(kStrings));

  source->AddLocalizedString("dangerDownloadDesc",
                             IDS_BLOCK_REASON_DANGEROUS_DOWNLOAD);
  source->AddLocalizedString(
      "dangerUncommonDesc",
      requests_ap_verdicts
          ? IDS_BLOCK_REASON_UNCOMMON_DOWNLOAD_IN_ADVANCED_PROTECTION
          : IDS_BLOCK_REASON_UNCOMMON_DOWNLOAD);
  source->AddLocalizedString("dangerSettingsDesc",
                             IDS_BLOCK_REASON_UNWANTED_DOWNLOAD);

  if (browser_defaults::kDownloadPageHasShowInFolder)
    source->AddLocalizedString("controlShowInFolder", IDS_DOWNLOAD_LINK_SHOW);

  // Build an Accelerator to describe undo shortcut
  // NOTE: the undo shortcut is also defined in downloads/downloads.html
  // TODO(crbug/893033): de-duplicate shortcut by moving all shortcut
  // definitions from JS to C++.
  ui::Accelerator undoAccelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_DOWNLOAD_UNDO_DESCRIPTION,
                                           undoAccelerator.GetShortcutText()));

  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean("allowDeletingHistory",
                     prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory) &&
                         !profile->IsSupervised());

  source->AddLocalizedString("inIncognito", IDS_DOWNLOAD_IN_INCOGNITO);

  source->AddResourcePath("images/incognito_marker.svg",
                          IDR_DOWNLOADS_IMAGES_INCOGNITO_MARKER_SVG);
  source->AddResourcePath("images/no_downloads.svg",
                          IDR_DOWNLOADS_IMAGES_NO_DOWNLOADS_SVG);
  source->AddResourcePath("downloads.mojom-lite.js",
                          IDR_DOWNLOADS_MOJO_LITE_JS);
#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePath("downloads.mojom-lite.html",
                          IDR_DOWNLOADS_MOJO_LITE_HTML);
  source->AddResourcePath("crisper.js", IDR_DOWNLOADS_CRISPER_JS);
  source->SetDefaultResource(IDR_DOWNLOADS_VULCANIZED_HTML);
#else
  for (size_t i = 0; i < kDownloadsResourcesSize; ++i) {
    source->AddResourcePath(kDownloadsResources[i].name,
                            kDownloadsResources[i].value);
  }
  // Add the subpage loader, to load subpages in non-optimized builds.
  source->AddResourcePath("subpage_loader.html", IDR_WEBUI_HTML_SUBPAGE_LOADER);
  source->AddResourcePath("subpage_loader.js", IDR_WEBUI_JS_SUBPAGE_LOADER);
  source->SetDefaultResource(IDR_DOWNLOADS_DOWNLOADS_HTML);
#endif

  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUI::DownloadsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true), page_factory_binding_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  // Set up the chrome://downloads/ source.
  content::WebUIDataSource* source = CreateDownloadsUIHTMLSource(profile);
  ManagedUIHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(profile, source);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  AddHandlerToRegistry(base::BindRepeating(&DownloadsUI::BindPageHandlerFactory,
                                           base::Unretained(this)));
}

DownloadsUI::~DownloadsUI() = default;

// static
base::RefCountedMemory* DownloadsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_DOWNLOADS_FAVICON, scale_factor);
}

void DownloadsUI::BindPageHandlerFactory(
    downloads::mojom::PageHandlerFactoryRequest request) {
  if (page_factory_binding_.is_bound())
    page_factory_binding_.Unbind();

  page_factory_binding_.Bind(std::move(request));
}

void DownloadsUI::CreatePageHandler(
    downloads::mojom::PagePtr page,
    downloads::mojom::PageHandlerRequest request) {
  DCHECK(page);
  Profile* profile = Profile::FromWebUI(web_ui());
  DownloadManager* dlm = BrowserContext::GetDownloadManager(profile);

  page_handler_.reset(new DownloadsDOMHandler(std::move(request),
                                              std::move(page), dlm, web_ui()));
}
