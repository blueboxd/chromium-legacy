// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/ui/quick_answers_view.h"

#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswerUiElement;
using chromeos::quick_answers::QuickAnswerUiElementType;

namespace ash {
namespace {
constexpr int kAssistantIconSizeDip = 16;
constexpr int kDefaultPaddingBelowDip = 10;
constexpr int kDefaultIconLeftPaddingDip = 8;
constexpr int kDefaultIconRightPaddingDip = 8;
constexpr int kDefaultIconUpperPaddingDip = 22;
constexpr int kDefaultLabelRightPaddingDip = 8;
constexpr int kDefaultTextUpperPaddingDip = 10;
constexpr int kLineHeightDip = 20;
constexpr int kHeightForOneRowAnswerDip = 60;
constexpr int kHeightForTwoRowAnswerDip = 75;

constexpr char kDefaultLoadingStr[] = "Loading...";
}  // namespace

// This class handles mouse events, and update background color or
// dismiss quick answers view.
class QuickAnswersViewHandler : public ui::EventHandler {
 public:
  explicit QuickAnswersViewHandler(QuickAnswersView* quick_answers_view)
      : quick_answers_view_(quick_answers_view) {
    // QuickAnswersView is a companion view of a menu. Menu host widget
    // sets mouse capture to receive all mouse events. Hence a pre-target
    // handler is needed to process mouse events for QuickAnswersView.
    Shell::Get()->AddPreTargetHandler(this);
  }

  ~QuickAnswersViewHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

  QuickAnswersViewHandler(const QuickAnswersViewHandler&) = delete;
  QuickAnswersViewHandler& operator=(const QuickAnswersViewHandler&) = delete;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    gfx::Point cursor_point =
        display::Screen::GetScreen()->GetCursorScreenPoint();
    gfx::Rect bounds =
        quick_answers_view_->GetWidget()->GetWindowBoundsInScreen();
    switch (event->type()) {
      case ui::ET_MOUSE_MOVED: {
        if (bounds.Contains(cursor_point)) {
          quick_answers_view_->SetBackgroundColor(SK_ColorGRAY);
        } else {
          quick_answers_view_->SetBackgroundColor(SK_ColorWHITE);
        }
        break;
      }
      case ui::ET_MOUSE_PRESSED: {
        if (event->IsOnlyLeftMouseButton() && bounds.Contains(cursor_point)) {
          quick_answers_view_->SendQuickAnswersQuery();
        }
        break;
      }
      default:
        break;
    }
  }

 private:
  QuickAnswersView* const quick_answers_view_;
};

QuickAnswersView::QuickAnswersView(const gfx::Rect& anchor_view_bounds,
                                   const std::string& title,
                                   QuickAnswersUiController* controller)
    : anchor_view_bounds_(anchor_view_bounds),
      controller_(controller),
      title_(title),
      quick_answers_view_handler_(
          std::make_unique<QuickAnswersViewHandler>(this)) {
  InitLayout();
  InitWidget();
}

QuickAnswersView::~QuickAnswersView() {
  Shell::Get()->RemovePreTargetHandler(this);
}

const char* QuickAnswersView::GetClassName() const {
  return "QuickAnswersView";
}

void QuickAnswersView::UpdateAnchorViewBounds(
    const gfx::Rect& anchor_view_bounds) {
  anchor_view_bounds_ = anchor_view_bounds;
  UpdateBounds();
}

void QuickAnswersView::UpdateView(const gfx::Rect& anchor_view_bounds,
                                  const QuickAnswer& quick_answer) {
  has_second_row_answer_ = !quick_answer.second_answer_row.empty();
  anchor_view_bounds_ = anchor_view_bounds;
  RemoveAllChildViews(true);
  UpdateChildViews(quick_answer);
  UpdateBounds();
}

void QuickAnswersView::SendQuickAnswersQuery() {
  controller_->OnQuickAnswersViewPressed();
}

void QuickAnswersView::SetBackgroundColor(SkColor color) {
  if (background_color_ == color)
    return;
  background_color_ = color;
  SetBackground(views::CreateSolidBackground(background_color_));
}
int QuickAnswersView::GetPreferredHeight() {
  return has_second_row_answer_ ? kHeightForTwoRowAnswerDip
                                : kHeightForOneRowAnswerDip;
}

void QuickAnswersView::InitLayout() {
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  // Add Assistant icon.
  auto* assistant_icon = AddChildView(std::make_unique<views::ImageView>());
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));
  assistant_icon->SetBoundsRect({kDefaultIconLeftPaddingDip,
                                 kDefaultIconUpperPaddingDip,
                                 kAssistantIconSizeDip, kAssistantIconSizeDip});
  // Add title
  int label_start = kDefaultIconLeftPaddingDip + kAssistantIconSizeDip +
                    kDefaultIconRightPaddingDip;

  // TODO(yanxiao):Add more padding if there is image on the right side.
  int label_max_width =
      anchor_view_bounds_.width() - label_start - kDefaultLabelRightPaddingDip;
  auto* label =
      AddChildView(std::make_unique<views::Label>(base::UTF8ToUTF16(title_)));
  label->SetMaximumWidth(label_max_width);
  label->SetBoundsRect({label_start, kDefaultTextUpperPaddingDip,
                        label->CalculatePreferredSize().width(),
                        kLineHeightDip});
  label->SetLineHeight(kLineHeightDip);

  // Add loading label
  // TODO(yanxiao): change the string to loading animation.
  auto* loading_label = AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(kDefaultLoadingStr)));
  loading_label->SetMaximumWidth(label_max_width);
  loading_label->SetFontList(gfx::FontList());
  loading_label->SetBoundsRect(
      {label_start, kDefaultTextUpperPaddingDip + kLineHeightDip,
       loading_label->CalculatePreferredSize().width(), kLineHeightDip});
  loading_label->SetLineHeight(kLineHeightDip);
}

void QuickAnswersView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.context = Shell::Get()->GetRootWindowForNewWindows();
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);
  UpdateBounds();
}

void QuickAnswersView::UpdateBounds() {
  // TODO(yanxiao): This part needs to be updated to handle corner cases.
  GetWidget()->SetBounds(gfx::Rect(
      anchor_view_bounds_.x(),
      anchor_view_bounds_.y() - kDefaultPaddingBelowDip - GetPreferredHeight(),
      anchor_view_bounds_.width(), GetPreferredHeight()));
}

void QuickAnswersView::UpdateChildViews(const QuickAnswer& quick_answer) {
  // Add Assistant icon.
  auto* assistant_icon = AddChildView(std::make_unique<views::ImageView>());
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));
  assistant_icon->SetBoundsRect({kDefaultIconLeftPaddingDip,
                                 kDefaultIconUpperPaddingDip,
                                 kAssistantIconSizeDip, kAssistantIconSizeDip});
  int start_y = kDefaultTextUpperPaddingDip;
  // Add title
  UpdateOneRowAnswer(quick_answer.title, start_y);

  // Add first row answer.
  start_y += kLineHeightDip;
  UpdateOneRowAnswer(quick_answer.first_answer_row, start_y);

  // Add second row answer.
  start_y += kLineHeightDip;
  if (quick_answer.second_answer_row.size() > 0) {
    UpdateOneRowAnswer(quick_answer.second_answer_row, start_y);
  }
}

void QuickAnswersView::UpdateOneRowAnswer(
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& answers,
    int y) {
  int label_start = kDefaultIconLeftPaddingDip + kAssistantIconSizeDip +
                    kDefaultIconRightPaddingDip;
  for (const auto& element : answers) {
    switch (element->type()) {
      case QuickAnswerUiElementType::kText: {
        QuickAnswerUiElement* ui_element = element.get();
        auto* text_element =
            static_cast<chromeos::quick_answers::QuickAnswerText*>(ui_element);
        auto* label = AddChildView(std::make_unique<views::Label>(
            base::UTF8ToUTF16(text_element->text_)));
        // TODO(yanxiao):Add more padding if there is image on the right side.
        int label_max_width = anchor_view_bounds_.width() - label_start -
                              kDefaultLabelRightPaddingDip;
        label->SetMaximumWidth(label_max_width);
        label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
        label->SetEnabledColor(text_element->color_);
        label->SetFontList(gfx::FontList());
        label->SetBoundsRect({label_start, y,
                              label->CalculatePreferredSize().width(),
                              kLineHeightDip});
        label->SetLineHeight(kLineHeightDip);
        label_start += label->CalculatePreferredSize().width() +
                       kDefaultLabelRightPaddingDip;
        break;
      }
      case QuickAnswerUiElementType::kImage:
        // TODO(yanxiao): Add image view
        break;
      default:
        break;
    }
  }
}

}  // namespace ash
