// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime/ime_warning_bubble_view.h"

#include <string>
#include <utility>

#include "chrome/browser/extensions/api/input_ime/input_ime_api_nonchromeos.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"

using extensions::Extension;

namespace {

// The column width of the warning bubble.
const int kColumnWidth = 285;

std::unique_ptr<views::Label> CreateExtensionNameLabel(
    const base::string16& text) {
  auto label = std::make_unique<views::Label>(text, CONTEXT_BODY_TEXT_SMALL);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

}  // namespace

// static
void ImeWarningBubbleView::ShowBubble(
    const extensions::Extension* extension,
    BrowserView* browser_view,
    const ImeWarningBubbleResponseCallback& callback) {
  // The ImeWarningBubbleView is self-owned, it deletes itself when the widget
  // is closed or the parent browser is destroyed.
  ImeWarningBubbleView::ime_warning_bubble_for_test_ =
      new ImeWarningBubbleView(extension, browser_view, callback);
}

void ImeWarningBubbleView::OnToolbarActionsBarAnimationEnded() {
  if (!bubble_has_shown_) {
    views::BubbleDialogDelegateView::CreateBubble(this)->Show();
    bubble_has_shown_ = true;
  }
}

void ImeWarningBubbleView::OnBrowserRemoved(Browser* browser) {
  // |browser_| is being destroyed before the bubble is successfully shown.
  if ((browser_ == browser) && !bubble_has_shown_)
    delete this;
}

// static
ImeWarningBubbleView* ImeWarningBubbleView::ime_warning_bubble_for_test_ =
    nullptr;

ImeWarningBubbleView::ImeWarningBubbleView(
    const extensions::Extension* extension,
    BrowserView* browser_view,
    const ImeWarningBubbleResponseCallback& callback)
    : extension_(extension),
      browser_view_(browser_view),
      browser_(browser_view->browser()),
      response_callback_(callback) {
  BrowserList::AddObserver(this);

  DialogDelegate::SetAcceptCallback(base::BindOnce(
      [](ImeWarningBubbleView* bubble) {
        const bool never_show = bubble->never_show_checkbox_->GetChecked();
        std::move(bubble->response_callback_)
            .Run(never_show
                     ? ImeWarningBubblePermissionStatus::GRANTED_AND_NEVER_SHOW
                     : ImeWarningBubblePermissionStatus::GRANTED);
      },
      base::Unretained(this)));
  DialogDelegate::SetCancelCallback(base::BindOnce(
      [](ImeWarningBubbleView* bubble) {
        std::move(bubble->response_callback_)
            .Run(ImeWarningBubblePermissionStatus::DENIED);
      },
      base::Unretained(this)));

  // The lifetime of this bubble is tied to the lifetime of the browser.
  set_parent_window(
      platform_util::GetViewForWindow(browser_view_->GetNativeWindow()));
  InitAnchorView();
  InitLayout();

  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    // TODO(pbos): During cleanup (default-enabling this), remove
    // OnBrowserRemoved and stop observing the browser. The widget will now
    // always be created and always have ownership.
    // TODO(pbos): Move widget creation outside this class when this is being
    // cleaned up. ::ShowBubble should create the Widget and queue showing it.
    bubble_has_shown_ = true;
    ExtensionsToolbarContainer* const container =
        browser_view_->toolbar_button_provider()
            ->GetExtensionsToolbarContainer();
    views::Widget* const widget =
        views::BubbleDialogDelegateView::CreateBubble(this);
    if (container) {
      container->ShowWidgetForExtension(widget, extension_->id());
    } else {
      widget->Show();
    }
    chrome::RecordDialogCreation(chrome::DialogIdentifier::IME_WARNING);
    return;
  }

  // If the toolbar is not animating, shows the warning bubble directly.
  // Otherwise, shows the bubble in method OnToolbarActionsBarAnimationEnded().
  if (IsToolbarAnimating()) {
    toolbar_actions_bar_observer_.Add(
        browser_view_->toolbar()->browser_actions()->toolbar_actions_bar());
    return;
  }
  views::BubbleDialogDelegateView::CreateBubble(this)->Show();
  bubble_has_shown_ = true;
  chrome::RecordDialogCreation(chrome::DialogIdentifier::IME_WARNING);
}

ImeWarningBubbleView::~ImeWarningBubbleView() {
  if (!response_callback_.is_null()) {
    std::move(response_callback_)
        .Run(ImeWarningBubblePermissionStatus::ABORTED);
  }

  BrowserList::RemoveObserver(this);
}

void ImeWarningBubbleView::InitAnchorView() {
  views::View* anchor_view = nullptr;
  if (anchor_to_action_) {
    ExtensionsToolbarContainer* const container =
        browser_view_->toolbar_button_provider()
            ->GetExtensionsToolbarContainer();
    if (container) {
      anchor_view = container->GetViewForId(extension_->id());
    } else if (!base::FeatureList::IsEnabled(
                   features::kExtensionsToolbarMenu)) {
      BrowserActionsContainer* const browser_actions_container =
          browser_view_->toolbar_button_provider()
              ->GetBrowserActionsContainer();
      ToolbarActionView* const reference_view =
          browser_actions_container->GetViewForId(extension_->id());
      if (reference_view && reference_view->GetVisible())
        anchor_view = reference_view;
    }
  }
  if (!anchor_view) {
    anchor_view = browser_view_->toolbar_button_provider()
                      ->GetDefaultExtensionDialogAnchorView();
  }
  SetAnchorView(anchor_view);
  SetArrow(views::BubbleBorder::TOP_RIGHT);
}

void ImeWarningBubbleView::InitLayout() {
  // The Ime warning bubble is like this:
  //
  // -----------------------------------------
  // | Warning info                          |
  // |                                       |
  // | [check box] Never show this again.    |
  // |                                       |
  // |                    --------  -------- |
  // |                    |CANCEL|  |  OK  | |
  // |                    --------  -------- |
  // -----------------------------------------
  //

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  int cs_id = 0;

  views::ColumnSet* main_cs = layout->AddColumnSet(cs_id);
  // The first row which shows the warning info.
  main_cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kFixed, kColumnWidth, 0);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  layout->StartRow(views::GridLayout::kFixedSize, cs_id);
  base::string16 extension_name = base::UTF8ToUTF16(extension_->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  std::unique_ptr<views::Label> warning =
      CreateExtensionNameLabel(l10n_util::GetStringFUTF16(
          IDS_IME_API_ACTIVATED_WARNING, extension_name));
  layout->AddView(std::move(warning));
  layout->AddPaddingRow(views::GridLayout::kFixedSize, vertical_spacing);

  // The seconde row which shows the check box.
  layout->StartRow(views::GridLayout::kFixedSize, cs_id);
  never_show_checkbox_ = layout->AddView(std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_IME_API_NEVER_SHOW)));
}

bool ImeWarningBubbleView::IsToolbarAnimating() {
  DCHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  return anchor_to_action_ &&
         browser_view_->toolbar()->browser_actions()->animating();
}
