// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace send_tab_to_self {

void CreateNewEntry(content::WebContents* tab,
                    const std::string& target_device_name,
                    const std::string& target_device_guid,
                    const GURL& link_url) {
  content::NavigationEntry* navigation_entry =
      tab->GetController().GetLastCommittedEntry();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  GURL url = navigation_entry->GetURL();
  std::string title = base::UTF16ToUTF8(navigation_entry->GetTitle());
  base::Time navigation_time = navigation_entry->GetTimestamp();

  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();

  UMA_HISTOGRAM_BOOLEAN("SendTabToSelf.Sync.ModelLoadedInTime",
                        model->IsReady());
  if (!model->IsReady()) {
    DesktopNotificationHandler(profile).DisplayFailureMessage(url);
    return;
  }

  const SendTabToSelfEntry* entry;
  if (link_url.is_valid()) {
    // When share a link.
    entry = model->AddEntry(link_url, "", base::Time(), target_device_guid);
  } else {
    // When share a tab.
    entry = model->AddEntry(url, title, navigation_time, target_device_guid);
  }
  if (entry) {
    DesktopNotificationHandler(profile).DisplaySendingConfirmation(
        *entry, target_device_name);
  } else {
    DesktopNotificationHandler(profile).DisplayFailureMessage(url);
  }
}

void ShareToSingleTarget(content::WebContents* tab, const GURL& link_url) {
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK_EQ(GetValidDeviceCount(profile), 1u);
  const std::vector<TargetDeviceInfo>& devices =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel()
          ->GetTargetDeviceInfoSortedList();
  CreateNewEntry(tab, devices.begin()->device_name, devices.begin()->cache_guid,
                 link_url);
}

gfx::ImageSkia* GetImageSkia() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  bool is_dark = native_theme && native_theme->SystemDarkModeEnabled();
  int resource_id = is_dark ? IDR_SEND_TAB_TO_SELF_ICON_DARK
                            : IDR_SEND_TAB_TO_SELF_ICON_LIGHT;
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
}

void RecordSendTabToSelfClickResult(const std::string& entry_point,
                                    SendTabToSelfClickResult state) {
  base::UmaHistogramEnumeration("SendTabToSelf." + entry_point + ".ClickResult",
                                state);
}

void RecordSendTabToSelfDeviceCount(const std::string& entry_point,
                                    const int& device_count) {
  base::UmaHistogramCounts100("SendTabToSelf." + entry_point + ".DeviceCount",
                              device_count);
}

size_t GetValidDeviceCount(Profile* profile) {
  SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  DCHECK(service);
  SendTabToSelfModel* model = service->GetSendTabToSelfModel();
  DCHECK(model);
  const std::vector<TargetDeviceInfo>& devices =
      model->GetTargetDeviceInfoSortedList();
  return devices.size();
}

base::string16 GetSingleTargetDeviceName(Profile* profile) {
  DCHECK_EQ(GetValidDeviceCount(profile), 1u);
  return base::UTF8ToUTF16(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel()
          ->GetTargetDeviceInfoSortedList()
          .begin()
          ->device_name);
}

}  // namespace send_tab_to_self
