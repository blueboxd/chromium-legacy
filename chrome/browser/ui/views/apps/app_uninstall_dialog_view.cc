// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_uninstall_dialog_view.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif

// static
void apps::UninstallDialog::UiBase::Create(
    Profile* profile,
    apps::mojom::AppType app_type,
    const std::string& app_id,
    gfx::ImageSkia image,
    apps::UninstallDialog* uninstall_dialog) {
  new AppUninstallDialogView(profile, app_type, app_id, image,
                             uninstall_dialog);
}

AppUninstallDialogView::AppUninstallDialogView(
    Profile* profile,
    apps::mojom::AppType app_type,
    const std::string& app_id,
    gfx::ImageSkia image,
    apps::UninstallDialog* uninstall_dialog)
    : apps::UninstallDialog::UiBase(image, uninstall_dialog),
      BubbleDialogDelegateView(nullptr, views::BubbleBorder::NONE) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  // Add margins for the icon plus the icon-title padding so that the dialog
  // contents align with the title text.
  set_margins(
      margins() +
      gfx::Insets(0, margins().left() + apps::UninstallDialog::kSizeHintInDip,
                  0, 0));

  InitializeView(profile, app_type, app_id);
  constrained_window::CreateBrowserModalDialogViews(this, nullptr)->Show();
}

bool AppUninstallDialogView::Cancel() {
  return Close();
}

bool AppUninstallDialogView::Accept() {
  const bool clear_site_data =
      clear_site_data_checkbox_ && clear_site_data_checkbox_->GetChecked();
  const bool report_abuse_checkbox =
      report_abuse_checkbox_ && report_abuse_checkbox_->GetChecked();
  uninstall_dialog()->OnDialogClosed(true /* uninstall */, clear_site_data,
                                     report_abuse_checkbox);
  return true;
}

bool AppUninstallDialogView::Close() {
  uninstall_dialog()->OnDialogClosed(false /* uninstall */,
                                     false /* clear_site_data */,
                                     false /* report_abuse */);
  return true;
}

gfx::Size AppUninstallDialogView::CalculatePreferredSize() const {
  const int default_width = views::LayoutProvider::Get()->GetDistanceMetric(
                                DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                            margins().width();
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

ui::ModalType AppUninstallDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

gfx::ImageSkia AppUninstallDialogView::GetWindowIcon() {
  return image();
}

base::string16 AppUninstallDialogView::GetWindowTitle() const {
  return window_title_;
}

bool AppUninstallDialogView::ShouldShowCloseButton() const {
  return false;
}

bool AppUninstallDialogView::ShouldShowWindowIcon() const {
  return true;
}

void AppUninstallDialogView::AddMultiLineLabel(
    views::View* parent,
    const base::string16& label_text) {
  auto* label =
      parent->AddChildView(std::make_unique<views::Label>(label_text));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
}

void AppUninstallDialogView::InitializeViewForExtension(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  DCHECK(extension);

  window_title_ =
      l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_TITLE,
                                 base::UTF8ToUTF16(extension->name()));

  if (extensions::ManifestURL::UpdatesFromGallery(extension)) {
    auto report_abuse_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_REPORT_ABUSE));
    report_abuse_checkbox->SetMultiLine(true);
    report_abuse_checkbox_ = AddChildView(std::move(report_abuse_checkbox));
  } else if (extension->from_bookmark()) {
    auto clear_site_data_checkbox =
        std::make_unique<views::Checkbox>(l10n_util::GetStringFUTF16(
            IDS_EXTENSION_UNINSTALL_PROMPT_REMOVE_DATA_CHECKBOX,
            url_formatter::FormatUrlForSecurityDisplay(
                extensions::AppLaunchInfo::GetFullLaunchURL(extension),
                url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)));
    clear_site_data_checkbox->SetMultiLine(true);
    clear_site_data_checkbox_ =
        AddChildView(std::move(clear_site_data_checkbox));
  }
}

#if defined(OS_CHROMEOS)
void AppUninstallDialogView::InitializeViewForArcApp(
    Profile* profile,
    const std::string& app_id) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id);
  DCHECK(arc_prefs);

  window_title_ = l10n_util::GetStringUTF16(
      app_info->shortcut ? IDS_EXTENSION_UNINSTALL_PROMPT_TITLE
                         : IDS_APP_UNINSTALL_PROMPT_TITLE);

  base::string16 heading_text = l10n_util::GetStringFUTF16(
      app_info->shortcut ? IDS_EXTENSION_UNINSTALL_PROMPT_HEADING
                         : IDS_NON_PLATFORM_APP_UNINSTALL_PROMPT_HEADING,
      base::UTF8ToUTF16(app_info->name));
  base::string16 subheading_text;
  if (!app_info->shortcut) {
    subheading_text = l10n_util::GetStringUTF16(
        IDS_ARC_APP_UNINSTALL_PROMPT_DATA_REMOVAL_WARNING);
  }

  if (!app_info->shortcut) {
    DialogDelegate::set_button_label(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_APP_BUTTON));
  }

  auto* text_container = AddChildView(std::make_unique<views::View>());
  auto* text_container_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  text_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  text_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddMultiLineLabel(text_container, heading_text);
  if (!subheading_text.empty())
    AddMultiLineLabel(text_container, subheading_text);
}
#endif

void AppUninstallDialogView::InitializeView(Profile* profile,
                                            apps::mojom::AppType app_type,
                                            const std::string& app_id) {
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kBuiltIn:
      NOTREACHED();
      break;
    case apps::mojom::AppType::kArc:
#if defined(OS_CHROMEOS)
      InitializeViewForArcApp(profile, app_id);
#endif
      break;
    case apps::mojom::AppType::kCrostini:
      NOTREACHED();
      break;
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      InitializeViewForExtension(profile, app_id);
      break;
  }
}
