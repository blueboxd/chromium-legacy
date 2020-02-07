// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_installed_bubble_view.h"

#include <algorithm>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble_model.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/command.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bubble/bubble_controller.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "extensions/common/extension.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

using extensions::Extension;

namespace {

const int kRightColumnWidth = 285;
constexpr gfx::Size kMaxIconSize{43, 43};

views::Label* CreateLabel(const base::string16& text) {
  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SizeToFit(kRightColumnWidth);
  return label;
}

views::View* AnchorViewForBrowser(const ExtensionInstalledBubbleModel* model,
                                  Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* reference_view = nullptr;

  if (model->anchor_to_action()) {
    if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
      ExtensionsToolbarContainer* const container =
          browser_view->toolbar_button_provider()
              ->GetExtensionsToolbarContainer();
      if (container)
        reference_view = container->GetViewForId(model->extension_id());
    } else {
      BrowserActionsContainer* container =
          browser_view->toolbar()->browser_actions();
      // Hitting this DCHECK means |ShouldShow| failed.
      DCHECK(container);
      DCHECK(!container->animating());

      reference_view = container->GetViewForId(model->extension_id());
    }
  } else if (model->anchor_to_omnibox()) {
    reference_view = browser_view->GetLocationBarView()->location_icon_view();
  }

  // Default case.
  if (!reference_view || !reference_view->GetVisible()) {
    return browser_view->toolbar_button_provider()
        ->GetDefaultExtensionDialogAnchorView();
  }
  return reference_view;
}

std::unique_ptr<views::View> CreateSigninPromoView(
    Profile* profile,
    BubbleSyncPromoDelegate* delegate) {
#if defined(OS_CHROMEOS)
  // ChromeOS does not show the signin promo.
  return nullptr;
#else
  return std::make_unique<DiceBubbleSyncPromoView>(
      profile, delegate,
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
      IDS_EXTENSION_INSTALLED_DICE_PROMO_SYNC_MESSAGE,
      /*dice_signin_button_prominent=*/true);
#endif
}

}  // namespace

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the pageAction icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The app menu. This case includes pageActions that don't
//                      specify a default icon.
class ExtensionInstalledBubbleView : public BubbleSyncPromoDelegate,
                                     public views::BubbleDialogDelegateView {
 public:
  ExtensionInstalledBubbleView(
      BubbleReference reference,
      Browser* browser,
      std::unique_ptr<ExtensionInstalledBubbleModel> model);
  ~ExtensionInstalledBubbleView() override;

  // Recalculate the anchor position for this bubble.
  void UpdateAnchorView();

  void CloseBubble(BubbleCloseReason reason);

  const ExtensionInstalledBubbleModel* model() const { return model_.get(); }

 private:
  // views::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;
  void Init() override;

  // BubbleSyncPromoDelegate:
  void OnEnableSync(const AccountInfo& account_info,
                    bool is_default_promo_account) override;

  void LinkClicked();

  BubbleReference bubble_reference_;

  Browser* const browser_;
  const std::unique_ptr<ExtensionInstalledBubbleModel> model_;
  gfx::ImageSkia icon_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleView);
};

ExtensionInstalledBubbleView::ExtensionInstalledBubbleView(
    BubbleReference bubble_reference,
    Browser* browser,
    std::unique_ptr<ExtensionInstalledBubbleModel> model)
    : BubbleDialogDelegateView(nullptr,
                               model->anchor_to_omnibox()
                                   ? views::BubbleBorder::TOP_LEFT
                                   : views::BubbleBorder::TOP_RIGHT),
      bubble_reference_(bubble_reference),
      browser_(browser),
      model_(std::move(model)),
      icon_(model_->MakeIconOfSize(kMaxIconSize)) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION_INSTALLED);
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
  if (model_->show_sign_in_promo()) {
    DialogDelegate::SetFootnoteView(
        CreateSigninPromoView(browser->profile(), this));
  }
}

ExtensionInstalledBubbleView::~ExtensionInstalledBubbleView() = default;

void ExtensionInstalledBubbleView::UpdateAnchorView() {
  views::View* reference_view = AnchorViewForBrowser(model_.get(), browser_);
  DCHECK(reference_view);
  SetAnchorView(reference_view);
}

void ExtensionInstalledBubbleView::CloseBubble(BubbleCloseReason reason) {
  // Tells the BubbleController to close the bubble to update the bubble's
  // status in BubbleManager. This does not circulate back to this method
  // because of the nullptr checks in place.
  if (bubble_reference_)
    bubble_reference_->CloseBubble(reason);

  GetWidget()->Close();
}

base::string16 ExtensionInstalledBubbleView::GetWindowTitle() const {
  // Add the heading (for all options).
  base::string16 extension_name = base::UTF8ToUTF16(model_->extension_name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                    extension_name);
}

gfx::ImageSkia ExtensionInstalledBubbleView::GetWindowIcon() {
  return icon_;
}

bool ExtensionInstalledBubbleView::ShouldShowWindowIcon() const {
  return true;
}

bool ExtensionInstalledBubbleView::ShouldShowCloseButton() const {
  return true;
}

void ExtensionInstalledBubbleView::Init() {
  UpdateAnchorView();

  // The Extension Installed bubble takes on various forms, depending on the
  // type of extension installed. In general, though, they are all similar:
  //
  // -------------------------
  // | Icon | Title      (x) |
  // |        Info           |
  // |        Extra info     |
  // -------------------------
  //
  // Icon and Title are always shown (as well as the close button).
  // Info is shown for browser actions, page actions and Omnibox keyword
  // extensions and might list keyboard shorcut for the former two types.
  // Extra info is...
  // ... for other types, either a description of how to manage the extension
  //     or a link to configure the keybinding shortcut (if one exists).
  // Extra info can include a promo for signing into sync.

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));
  layout->set_minimum_cross_axis_size(kRightColumnWidth);
  // Indent by the size of the icon.
  layout->set_inside_border_insets(gfx::Insets(
      0,
      icon_.width() +
          provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL),
      0, 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  SetLayoutManager(std::move(layout));

  if (model_->show_how_to_use())
    AddChildView(CreateLabel(model_->GetHowToUseText()));

  if (model_->show_key_binding()) {
    auto* manage_shortcut = AddChildView(std::make_unique<views::Link>(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS)));
    manage_shortcut->set_callback(base::BindRepeating(
        &ExtensionInstalledBubbleView::LinkClicked, base::Unretained(this)));
    manage_shortcut->SetUnderline(false);
  }

  if (model_->show_how_to_manage()) {
    AddChildView(CreateLabel(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO)));
  }
}

void ExtensionInstalledBubbleView::OnEnableSync(const AccountInfo& account,
                                                bool is_default_promo_account) {
  signin_ui_util::EnableSyncFromPromo(
      browser_, account,
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
      is_default_promo_account);
  CloseBubble(BUBBLE_CLOSE_NAVIGATED);
}

void ExtensionInstalledBubbleView::LinkClicked() {
  const GURL kUrl(base::StrCat({chrome::kChromeUIExtensionsURL,
                                chrome::kExtensionConfigureCommandsSubPage}));
  NavigateParams params = GetSingletonTabNavigateParams(browser_, kUrl);
  Navigate(&params);
  CloseBubble(BUBBLE_CLOSE_NAVIGATED);
}

ExtensionInstalledBubbleUi::ExtensionInstalledBubbleUi(
    ExtensionInstalledBubble* bubble)
    : bubble_(bubble), bubble_view_(nullptr) {
  DCHECK(bubble_);
}

ExtensionInstalledBubbleUi::~ExtensionInstalledBubbleUi() {
  if (bubble_view_)
    bubble_view_->GetWidget()->RemoveObserver(this);
}

void ExtensionInstalledBubbleUi::Show(BubbleReference bubble_reference) {
  bubble_view_ = new ExtensionInstalledBubbleView(
      bubble_reference, bubble_->browser(),
      std::make_unique<ExtensionInstalledBubbleModel>(
          bubble_->browser()->profile(), bubble_->extension(),
          bubble_->icon()));
  bubble_reference_ = bubble_reference;

  views::Widget* const widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  // When the extension is installed to the ExtensionsToolbarContainer, use the
  // container to pop out the extension icon and show the widget. Otherwise show
  // the widget directly.
  if (bubble_view_->model()->anchor_to_action() &&
      base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    ExtensionsToolbarContainer* const container =
        BrowserView::GetBrowserViewForBrowser(bubble_->browser())
            ->toolbar_button_provider()
            ->GetExtensionsToolbarContainer();
    if (container) {
      container->ShowWidgetForExtension(widget, bubble_->extension()->id());
    } else {
      widget->Show();
    }
  } else {
    widget->Show();
  }
  bubble_view_->GetWidget()->AddObserver(this);
}

void ExtensionInstalledBubbleUi::Close() {
  if (bubble_view_)
    bubble_view_->CloseBubble(BUBBLE_CLOSE_USER_DISMISSED);
}

void ExtensionInstalledBubbleUi::UpdateAnchorPosition() {
  DCHECK(bubble_view_);
  bubble_view_->UpdateAnchorView();
}

void ExtensionInstalledBubbleUi::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  bubble_view_ = nullptr;

  // Tells the BubbleController to close the bubble to update the bubble's
  // status in BubbleManager.
  if (bubble_reference_)
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST);
}

// Views (BrowserView) specific implementation.
bool ExtensionInstalledBubble::ShouldShow() {
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu))
    return true;
  if (anchor_position() == ANCHOR_ACTION) {
    BrowserActionsContainer* container =
        BrowserView::GetBrowserViewForBrowser(browser_)
            ->toolbar()
            ->browser_actions();
    return container && !container->animating();
  }
  return true;
}

std::unique_ptr<BubbleUi> ExtensionInstalledBubble::BuildBubbleUi() {
  return base::WrapUnique(new ExtensionInstalledBubbleUi(this));
}
