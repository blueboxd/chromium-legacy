// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/ui/quick_answers_view.h"

#include "ash/public/cpp/assistant/assistant_interface_binder.h"
#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswerText;
using chromeos::quick_answers::QuickAnswerUiElement;
using chromeos::quick_answers::QuickAnswerUiElementType;
using views::Button;
using views::Label;
using views::View;

// Spacing between this view and the anchor view.
constexpr int kMarginDip = 10;

constexpr gfx::Insets kMainViewInsets(4, 0);
constexpr gfx::Insets kContentViewInsets(8, 0, 8, 26);
constexpr float kHoverStateAlpha = 0.06f;

// Assistant icon.
constexpr int kAssistantIconSizeDip = 16;
constexpr gfx::Insets kAssistantIconInsets(10, 10, 0, 8);

// Spacing between lines in the main view.
constexpr int kLineSpacingDip = 4;
constexpr int kLineHeightDip = 20;

// Spacing between labels in the horizontal elements view.
constexpr int kLabelSpacingDip = 2;

// TODO(llin): Move to grd after confirming specs (b/149758492).
constexpr char kDefaultLoadingStr[] = "Loading...";
constexpr char kDefaultRetryStr[] = "Retry";
constexpr char kNetworkErrorStr[] = "Cannot connect to internet.";

// Dogfood button.
constexpr int kDogfoodButtonMarginDip = 4;
constexpr int kDogfoodButtonSizeDip = 20;
constexpr SkColor kDogfoodButtonColor = gfx::kGoogleGrey500;

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

// QuickAnswersViewHandler ----------------------------------------------------

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

  // Disallow copy and assign.
  QuickAnswersViewHandler(const QuickAnswersViewHandler&) = delete;
  QuickAnswersViewHandler& operator=(const QuickAnswersViewHandler&) = delete;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (!event->IsLocatedEvent())
      return;

    // Clone event and forward down the event-hierarchy.
    ui::LocatedEvent* clone =
        ui::Event::Clone(*event).release()->AsLocatedEvent();
    ui::Event::DispatcherApi(clone).set_target(event->target());
    DoDispatchEvent(quick_answers_view_, clone);

    // Show tooltips.
    auto* tooltip_manager =
        quick_answers_view_->GetWidget()->GetTooltipManager();
    if (tooltip_manager)
      tooltip_manager->UpdateTooltip();

    // Do not dismiss context menu for clicks inside the view.
    auto location = clone->target()->GetScreenLocation(*clone);
    if (quick_answers_view_->GetBoundsInScreen().Contains(location))
      event->StopPropagation();
  }

 private:
  bool DoDispatchEvent(views::View* view, ui::LocatedEvent* event) {
    if (event->handled())
      return true;

    // Convert |event| to local coordinates of |view|.
    gfx::Point location = event->target()->GetScreenLocation(*event);
    views::View::ConvertPointFromScreen(view, &location);
    event->set_location(location);
    ui::Event::DispatcherApi(event).set_target(view);

    // Process event and dispatch on children recursively.
    view->OnEvent(event);
    for (auto* child : view->children()) {
      if (DoDispatchEvent(child, event))
        return true;
    }
    return false;
  }

  QuickAnswersView* const quick_answers_view_;
};

// QuickAnswersView -----------------------------------------------------------

QuickAnswersView::QuickAnswersView(const gfx::Rect& anchor_view_bounds,
                                   const std::string& title,
                                   QuickAnswersUiController* controller)
    : Button(this),
      anchor_view_bounds_(anchor_view_bounds),
      controller_(controller),
      title_(title),
      quick_answers_view_handler_(
          std::make_unique<QuickAnswersViewHandler>(this)) {
  InitLayout();
  InitWidget();

  // Allow tooltips to be shown despite menu-controller owning capture.
  GetWidget()->SetNativeWindowProperty(
      views::TooltipManager::kGroupingPropertyKey,
      reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));
}

QuickAnswersView::~QuickAnswersView() {
  Shell::Get()->RemovePreTargetHandler(this);
}

const char* QuickAnswersView::GetClassName() const {
  return "QuickAnswersView";
}

void QuickAnswersView::StateChanged(views::Button::ButtonState old_state) {
  switch (state()) {
    case Button::ButtonState::STATE_NORMAL: {
      main_view_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
      break;
    }
    case Button::ButtonState::STATE_HOVERED: {
      if (!retry_label_)
        main_view_->SetBackground(views::CreateBackgroundFromPainter(
            views::Painter::CreateSolidRoundRectPainter(
                SkColorSetA(SK_ColorBLACK, kHoverStateAlpha * 0xFF),
                /*radius=*/0, kMainViewInsets)));
      break;
    }
    default:
      break;
  }
}

void QuickAnswersView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == dogfood_button_) {
    controller_->OnDogfoodButtonPressed();
    return;
  }
  if (sender == retry_label_) {
    controller_->OnRetryLabelPressed();
    return;
  }
  if (sender == this) {
    SendQuickAnswersQuery();
    return;
  }
}

void QuickAnswersView::SendQuickAnswersQuery() {
  controller_->OnQuickAnswersViewPressed();
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
  main_view_->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));

  // Add title.
  AddTextElement({title_}, content_view_);

  // Add error label.
  std::vector<std::unique_ptr<QuickAnswerUiElement>> description_labels;
  description_labels.push_back(
      std::make_unique<QuickAnswerText>(kNetworkErrorStr, gfx::kGoogleGrey700));
  auto* description_container =
      AddHorizontalUiElements(description_labels, content_view_);

  // Add retry label.
  retry_label_ =
      description_container->AddChildView(std::make_unique<views::LabelButton>(
          /*listener=*/this, base::UTF8ToUTF16(kDefaultRetryStr)));
  retry_label_->SetEnabledTextColors(gfx::kGoogleBlue600);
}

void QuickAnswersView::AddAssistantIcon() {
  // Add Assistant icon.
  auto* assistant_icon =
      main_view_->AddChildView(std::make_unique<views::ImageView>());
  assistant_icon->SetBorder(views::CreateEmptyBorder(kAssistantIconInsets));
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));
}

void QuickAnswersView::AddDogfoodButton() {
  auto* dogfood_view = AddChildView(std::make_unique<View>());
  auto* layout =
      dogfood_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(kDogfoodButtonMarginDip)));
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);
  auto dogfood_button = std::make_unique<views::ImageButton>(/*listener=*/this);
  dogfood_button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kDogfoodIcon, kDogfoodButtonSizeDip,
                            kDogfoodButtonColor));
  dogfood_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_ANSWERS_DOGFOOD_BUTTON_TOOLTIP_TEXT));
  dogfood_button_ = dogfood_view->AddChildView(std::move(dogfood_button));
}

void QuickAnswersView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  main_view_ = AddChildView(std::make_unique<View>());
  auto* layout =
      main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kMainViewInsets));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Add Assistant icon.
  AddAssistantIcon();

  // Add content view.
  content_view_ = main_view_->AddChildView(std::make_unique<View>());
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kContentViewInsets,
      kLineSpacingDip));
  AddTextElement({title_}, content_view_);
  AddTextElement({kDefaultLoadingStr, gfx::kGoogleGrey700}, content_view_);

  // Add dogfood button, if in dogfood.
  if (chromeos::features::IsQuickAnswersDogfood())
    AddDogfoodButton();
}

void QuickAnswersView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
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
