// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/undo_window.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/input_method/ui/border_factory.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/wm/core/window_animations.h"

namespace ui {
namespace ime {

namespace {
const char kUndoButtonText[] = "Undo";
}  // namespace

UndoWindow::UndoWindow(gfx::NativeView parent, AssistiveDelegate* delegate)
    : delegate_(delegate) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  undo_button_ = AddChildView(std::make_unique<views::LabelButton>(
      this, base::UTF8ToUTF16(kUndoButtonText)));
  undo_button_->SetImageLabelSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL));
}

void UndoWindow::OnThemeChanged() {
  undo_button_->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kAutocorrectUndoIcon,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_DefaultIconColor)));
  BubbleDialogDelegateView::OnThemeChanged();
}

UndoWindow::~UndoWindow() = default;

views::Widget* UndoWindow::InitWidget() {
  views::Widget* widget = BubbleDialogDelegateView::CreateBubble(this);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);

  GetBubbleFrameView()->SetBubbleBorder(
      GetBorderForWindow(WindowBorderType::Undo));
  GetBubbleFrameView()->OnThemeChanged();
  return widget;
}

void UndoWindow::Hide() {
  GetWidget()->Close();
}

void UndoWindow::Show() {
  GetWidget()->Show();
}

void UndoWindow::SetBounds(const gfx::Rect& word_bounds) {
  SetAnchorRect(word_bounds);
}

void UndoWindow::ButtonPressed(views::Button* sender, const ui::Event& event) {
  button_pressed_ = sender;
  if (sender == undo_button_) {
    AssistiveWindowButton button;
    button.id = ButtonId::kUndo;
    button.window_type = AssistiveWindowType::kUndoWindow;
    delegate_->AssistiveWindowButtonClicked(button);
  }
}

views::Button* UndoWindow::GetUndoButtonForTesting() {
  return undo_button_;
}

const char* UndoWindow::GetClassName() const {
  return "UndoWindow";
}

}  // namespace ime
}  // namespace ui
