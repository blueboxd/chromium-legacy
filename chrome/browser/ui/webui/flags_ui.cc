// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/flags_ui_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/flags_ui/flags_ui_constants.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/owner_flags_storage.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user_manager.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateFlagsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFlagsHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  source->AddLocalizedString(flags_ui::kFlagsRestartNotice,
                             IDS_FLAGS_UI_RELAUNCH_NOTICE);
  source->AddString(flags_ui::kVersion, version_info::GetVersionNumber());

#if defined(OS_CHROMEOS)
  if (!user_manager::UserManager::Get()->IsCurrentUserOwner() &&
      base::SysInfo::IsRunningOnChromeOS()) {
    // Set the string to show which user can actually change the flags.
    std::string owner;
    chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
    source->AddString(flags_ui::kOwnerEmail, base::UTF8ToUTF16(owner));
  } else {
    // The warning will be only shown on ChromeOS, when the current user is not
    // the owner.
    source->AddString(flags_ui::kOwnerEmail, base::string16());
  }
#endif

  source->AddResourcePath(flags_ui::kFlagsJS, IDR_FLAGS_UI_FLAGS_JS);
  source->SetDefaultResource(IDR_FLAGS_UI_FLAGS_HTML);
  return source;
}

#if defined(OS_CHROMEOS)
// On ChromeOS verifying if the owner is signed in is async operation and only
// after finishing it the UI can be properly populated. This function is the
// callback for whether the owner is signed in. It will respectively pick the
// proper PrefService for the flags interface.
void FinishInitialization(base::WeakPtr<FlagsUI> flags_ui,
                          Profile* profile,
                          FlagsUIHandler* dom_handler,
                          bool current_user_is_owner) {
  DCHECK(!profile->IsOffTheRecord());
  // If the flags_ui has gone away, there's nothing to do.
  if (!flags_ui)
    return;

  // On Chrome OS the owner can set system wide flags and other users can only
  // set flags for their own session.
  // Note that |dom_handler| is owned by the web ui that owns |flags_ui|, so
  // it is still alive if |flags_ui| is.
  if (current_user_is_owner) {
    chromeos::OwnerSettingsServiceChromeOS* service =
        chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
            profile);
    dom_handler->Init(new chromeos::about_flags::OwnerFlagsStorage(
                          profile->GetPrefs(), service),
                      flags_ui::kOwnerAccessToFlags);
  } else {
    dom_handler->Init(
        new flags_ui::PrefServiceFlagsStorage(profile->GetPrefs()),
        flags_ui::kGeneralAccessFlagsOnly);
  }
}
#endif

}  // namespace

FlagsUI::FlagsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  auto handler_owner = std::make_unique<FlagsUIHandler>();
  FlagsUIHandler* handler = handler_owner.get();
  web_ui->AddMessageHandler(std::move(handler_owner));

#if defined(OS_CHROMEOS)
  // Bypass possible incognito profile.
  Profile* original_profile = profile->GetOriginalProfile();
  if (base::SysInfo::IsRunningOnChromeOS() &&
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          original_profile)) {
    chromeos::OwnerSettingsServiceChromeOS* service =
        chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
            original_profile);
    service->IsOwnerAsync(base::Bind(&FinishInitialization,
                                     weak_factory_.GetWeakPtr(),
                                     original_profile, handler));
  } else {
    FinishInitialization(weak_factory_.GetWeakPtr(), original_profile, handler,
                         false /* current_user_is_owner */);
  }
#else
  handler->Init(
      new flags_ui::PrefServiceFlagsStorage(g_browser_process->local_state()),
      flags_ui::kOwnerAccessToFlags);
#endif

  // Set up the about:flags source.
  content::WebUIDataSource::Add(profile, CreateFlagsUIHTMLSource());
}

FlagsUI::~FlagsUI() {
}

// static
base::RefCountedMemory* FlagsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_FLAGS_FAVICON, scale_factor);
}
