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
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswerText;
using chromeos::quick_answers::QuickAnswerUiElement;
using chromeos::quick_answers::QuickAnswerUiElementType;
using views::Label;
using views::View;

// Spacing between this view and the anchor view.
constexpr int kMarginDip = 10;

constexpr gfx::Insets kMainViewInsets(16, 0, 16, 18);

constexpr int kAssistantIconSizeDip = 16;
constexpr gfx::Insets kAssistantIconInsets(2, 10, 0, 8);

// Spacing between lines in the main view.
constexpr int kLineSpacingDip = 4;
constexpr int kLineHeightDip = 20;

// Spacing between labels in the horizontal elements view.
constexpr int kLabelSpacingDip = 2;

constexpr char kDefaultLoadingStr[] = "Loading...";
constexpr char kDefaultRetryStr[] = "Retry";
constexpr char kNetworkErrorStr[] = "Cannot connect to internet.";

// Adds |text_element| as label to the container.
Label* AddTextElement(const QuickAnswerText& text_element, View* container) {
  auto* label =
      container->AddChildView(std::make_unique<Label>(text_element.text));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetEnabledColor(text_element.color);
  label->SetLineHeight(kLineHeightDip);
  return label;
}

// Adds the list of |QuickAnswerUiElement| horizontally to the container.
View* AddHorizontalUiElements(
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements,
    View* container) {
  auto* labels_container =
      container->AddChildView(std::make_unique<views::View>());
  labels_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kLabelSpacingDip));

  for (const auto& element : elements) {
    switch (element->type) {
      case QuickAnswerUiElementType::kText:
        AddTextElement(*static_cast<QuickAnswerText*>(element.get()),
                       labels_container);
        break;
      case QuickAnswerUiElementType::kImage:
        // TODO(yanxiao): Add image view
        break;
      default:
        break;
    }
  }

  return labels_container;
}

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
        if (quick_answers_view_->HasRetryLabel())
          return;
        if (bounds.Contains(cursor_point)) {
          quick_answers_view_->SetBackgroundColor(SK_ColorLTGRAY);
        } else {
          quick_answers_view_->SetBackgroundColor(SK_ColorWHITE);
        }
        break;
      }
      case ui::ET_MOUSE_PRESSED: {
        if (event->IsOnlyLeftMouseButton() && bounds.Contains(cursor_point)) {
          if (quick_answers_view_->HasRetryLabel()) {
            if (quick_answers_view_->WithinRetryLabelBounds(cursor_point)) {
              quick_answers_view_->OnRetryLabelPressed();
            }
            event->StopPropagation();
          } else {
            quick_answers_view_->SendQuickAnswersQuery();
          }
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

bool QuickAnswersView::HasRetryLabel() const {
  return retry_label_;
}

void QuickAnswersView::OnRetryLabelPressed() {
  controller_->OnRetryLabelPressed();
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

bool QuickAnswersView::WithinRetryLabelBounds(
    const gfx::Point& point_in_screen) const {
  return retry_label_ &&
         retry_label_->GetBoundsInScreen().Contains(point_in_screen);
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
  retry_label_ = nullptr;

  UpdateQuickAnswerResult(quick_answer);
  UpdateBounds();
}

void QuickAnswersView::ShowRetryView() {
  if (retry_label_)
    return;

  content_view_->RemoveAllChildViews(true);

  // Add title.
  AddTextElement({title_}, content_view_);

  // Add error label.
  std::vector<std::unique_ptr<QuickAnswerUiElement>> description_labels;
  description_labels.push_back(
      std::make_unique<QuickAnswerText>(kNetworkErrorStr, gfx::kGoogleGrey700));
  auto* description_container =
      AddHorizontalUiElements(description_labels, content_view_);

  // Add retry label.
  retry_label_ = AddTextElement({kDefaultRetryStr, gfx::kGoogleBlue600},
                                description_container);
}

void QuickAnswersView::AddAssistantIcon() {
  // Add Assistant icon.
  auto* assistant_icon = AddChildView(std::make_unique<views::ImageView>());
  assistant_icon->SetBorder(views::CreateEmptyBorder(kAssistantIconInsets));
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));
}

void QuickAnswersView::InitLayout() {
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kMainViewInsets));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddAssistantIcon();

  // Add content view.
  content_view_ = AddChildView(std::make_unique<views::View>());
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kLineSpacingDip));

  // Add title.
  AddTextElement({title_}, content_view_);

  // Add loading place holder.
  AddTextElement({kDefaultLoadingStr, gfx::kGoogleGrey700}, content_view_);
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
  int height = GetHeightForWidth(anchor_view_bounds_.width());
  int y = anchor_view_bounds_.y() - kMarginDip - height;
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(anchor_view_bounds_)
              .bounds()
              .y()) {
    // The Quick Answers view will be off screen if showing above the anchor.
    // Show below the anchor instead.
    y = anchor_view_bounds_.bottom() + kMarginDip;
  }

  GetWidget()->SetBounds(gfx::Rect(anchor_view_bounds_.x(), y,
                                   anchor_view_bounds_.width(), height));
}

void QuickAnswersView::UpdateQuickAnswerResult(
    const QuickAnswer& quick_answer) {
  content_view_->RemoveAllChildViews(true);

  // Add title.
  AddHorizontalUiElements(quick_answer.title, content_view_);

  // Add first row answer.
  if (quick_answer.first_answer_row.size() > 0)
    AddHorizontalUiElements(quick_answer.first_answer_row, content_view_);

  // Add second row answer.
  if (quick_answer.second_answer_row.size() > 0)
    AddHorizontalUiElements(quick_answer.second_answer_row, content_view_);
}

}  // namespace ash
