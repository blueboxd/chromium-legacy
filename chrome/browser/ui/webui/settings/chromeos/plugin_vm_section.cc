// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/plugin_vm_section.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {

PluginVmSection::PluginVmSection(Profile* profile,
                                 Delegate* per_page_delegate,
                                 PrefService* pref_service)
    : OsSettingsSection(profile, per_page_delegate),
      pref_service_(pref_service) {}

PluginVmSection::~PluginVmSection() = default;

void PluginVmSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"pluginVmPageTitle", IDS_SETTINGS_PLUGIN_VM_PAGE_TITLE},
      {"pluginVmPageLabel", IDS_SETTINGS_PLUGIN_VM_PAGE_LABEL},
      {"pluginVmPageSubtext", IDS_SETTINGS_PLUGIN_VM_PAGE_SUBTEXT},
      {"pluginVmPageEnable", IDS_SETTINGS_TURN_ON},
      {"pluginVmPrinterAccess", IDS_SETTINGS_PLUGIN_VM_PRINTER_ACCESS},
      {"pluginVmSharedPaths", IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS},
      {"pluginVmSharedPathsListHeading",
       IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS_LIST_HEADING},
      {"pluginVmSharedPathsInstructionsAdd",
       IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS_INSTRUCTIONS_ADD},
      {"pluginVmSharedPathsInstructionsRemove",
       IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS_INSTRUCTIONS_REMOVE},
      {"pluginVmSharedPathsRemoveSharing",
       IDS_SETTINGS_PLUGIN_VM_SHARED_PATHS_REMOVE_SHARING},
      {"pluginVmRemove", IDS_SETTINGS_PLUGIN_VM_REMOVE_LABEL},
      {"pluginVmRemoveButton", IDS_SETTINGS_PLUGIN_VM_REMOVE_BUTTON},
      {"pluginVmRemoveConfirmationDialogMessage",
       IDS_SETTINGS_PLUGIN_VM_CONFIRM_REMOVE_DIALOG_BODY},
      {"pluginVmCameraAccessTitle", IDS_SETTINGS_PLUGIN_VM_CAMERA_ACCESS_TITLE},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
  html_source->AddBoolean(
      "showPluginVmCamera",
      base::FeatureList::IsEnabled(features::kPluginVmShowCameraSetting));

  // If |!allow_plugin_vm| we still want to |show_plugin_vm| if the VM image is
  // on disk, so that users are still able to delete the image at will.
  const bool allow_plugin_vm =
      plugin_vm::IsPluginVmAllowedForProfile(profile());
  html_source->AddBoolean("allowPluginVm", allow_plugin_vm);
  html_source->AddBoolean(
      "showPluginVm",
      allow_plugin_vm ||
          pref_service_->GetBoolean(plugin_vm::prefs::kPluginVmImageExists));
}

}  // namespace settings
}  // namespace chromeos
