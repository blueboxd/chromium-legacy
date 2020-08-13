// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_model_host.h"

#include <utility>

#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace views {
namespace {
constexpr int kColumnId = 0;

DialogContentType FieldTypeToContentType(ui::DialogModelField::Type type) {
  switch (type) {
    case ui::DialogModelField::kButton:
      return DialogContentType::CONTROL;
    case ui::DialogModelField::kTextfield:
      return DialogContentType::CONTROL;
    case ui::DialogModelField::kCombobox:
      return DialogContentType::CONTROL;
  }
  NOTREACHED();
  return DialogContentType::CONTROL;
}

}  // namespace

BubbleDialogModelHost::BubbleDialogModelHost(
    std::unique_ptr<ui::DialogModel> model)
    : model_(std::move(model)) {
  model_->set_host(GetPassKey(), this);

  ConfigureGridLayout();

  SetAcceptCallback(base::BindOnce(&ui::DialogModel::OnDialogAccepted,
                                   base::Unretained(model_.get()),
                                   GetPassKey()));
  SetCancelCallback(base::BindOnce(&ui::DialogModel::OnDialogCancelled,
                                   base::Unretained(model_.get()),
                                   GetPassKey()));
  SetCloseCallback(base::BindOnce(&ui::DialogModel::OnDialogClosed,
                                  base::Unretained(model_.get()),
                                  GetPassKey()));
  RegisterWindowClosingCallback(
      base::BindOnce(&ui::DialogModel::OnWindowClosing,
                     base::Unretained(model_.get()), GetPassKey()));

  int button_mask = ui::DIALOG_BUTTON_NONE;

  // TODO(pbos): Separate dialog buttons from fields. This is not nice.
  for (const auto& field : model_->fields(GetPassKey())) {
    if (field->type(GetPassKey()) != ui::DialogModelField::kButton)
      continue;

    const auto* button = static_cast<const ui::DialogModelButton*>(field.get());
    if (field->model_field_id(GetPassKey()) > ui::DIALOG_BUTTON_LAST) {
      DCHECK_EQ(button, model_->GetExtraButton());
      auto extra_button =
          std::make_unique<views::MdTextButton>(this, button->label());
      extra_button->SetID(field->model_field_id(GetPassKey()));
      SetExtraView(std::move(extra_button));

      continue;
    }

    button_mask |= field->model_field_id(GetPassKey());
    SetButtons(button_mask);
    if (!button->label().empty()) {
      SetButtonLabel(
          static_cast<ui::DialogButton>(field->model_field_id(GetPassKey())),
          button->label());
    }
  }

  // Populate dialog using the observer functions to make sure they use the same
  // code path as updates.
  OnModelChanged(model_.get());
}

BubbleDialogModelHost::~BubbleDialogModelHost() {
  // Remove children as they may refer to the soon-to-be-destructed model.
  RemoveAllChildViews(true);
}

View* BubbleDialogModelHost::GetInitiallyFocusedView() {
  if (model_->initially_focused_field(GetPassKey())) {
    // TODO(pbos): Update this so that it works for dialog buttons.
    View* focused_view = GetViewByID(
        model_
            ->GetFieldByUniqueId(*model_->initially_focused_field(GetPassKey()))
            ->model_field_id(GetPassKey()));
    // The dialog should be populated now so this should correspond to a view
    // with this ID.
    DCHECK(focused_view);
    return focused_view;
  }
  return BubbleDialogDelegateView::GetInitiallyFocusedView();
}

void BubbleDialogModelHost::OnDialogInitialized() {
  UpdateAccelerators();
}

void BubbleDialogModelHost::Close() {
  // TODO(pbos): Synchronously destroy model here, as-if closing immediately.
  DCHECK(GetWidget());
  GetWidget()->Close();
}

void BubbleDialogModelHost::SelectAllText(int unique_id) {
  static_cast<Textfield*>(
      GetViewByID(model_->GetTextfieldByUniqueId(unique_id)->model_field_id(
          GetPassKey())))
      ->SelectAll(false);
}

void BubbleDialogModelHost::OnModelChanged(ui::DialogModel* model) {
  DCHECK(model == model_.get());
  WidgetDelegate::SetTitle(model->title(GetPassKey()));
  WidgetDelegate::SetShowCloseButton(model->show_close_button(GetPassKey()));

  // TODO(pbos): When fixing the DCHECK below, keep views and update them. This
  // is required to maintain view focus, for instance. Do not remove all
  // children and recreate. Needs to dynamically insert/remove GridLayout rows.
  DCHECK(children().empty()) << "TODO(pbos): Support changing the model after "
                                "host creation...";

  bool first_row = true;
  const auto& fields = model->fields(GetPassKey());
  const DialogContentType first_field_content_type =
      fields.empty()
          ? DialogContentType::CONTROL
          : FieldTypeToContentType(fields.front()->type(GetPassKey()));
  DialogContentType last_field_content_type = first_field_content_type;
  for (const auto& field : fields) {
    // TODO(pbos): This needs to take previous field type + next field type into
    // account to do this properly.
    if (!first_row) {
      // TODO(pbos): Move DISTANCE_CONTROL_LIST_VERTICAL to
      // views::LayoutProvider and replace "12" here.
      GetGridLayout()->AddPaddingRow(GridLayout::kFixedSize, 12);
    }

    View* last_field = nullptr;
    switch (field->type(GetPassKey())) {
      case ui::DialogModelField::kButton:
        // TODO(pbos): Add support for buttons that are part of content area.
        continue;
      case ui::DialogModelField::kTextfield:
        last_field = AddOrUpdateTextfield(
            static_cast<const ui::DialogModelTextfield&>(*field));
        break;

      case ui::DialogModelField::kCombobox:
        last_field = AddOrUpdateCombobox(
            static_cast<ui::DialogModelCombobox*>(field.get()));
        break;
    }
    DCHECK(last_field);
    last_field->SetID(field->model_field_id(GetPassKey()));
    last_field_content_type = FieldTypeToContentType(field->type(GetPassKey()));
    // TODO(pbos): Update logic here when mixing types.
    first_row = false;
  }

  set_margins(LayoutProvider::Get()->GetDialogInsetsForContentType(
      first_field_content_type, last_field_content_type));

  UpdateAccelerators();
}

GridLayout* BubbleDialogModelHost::GetGridLayout() {
  return static_cast<GridLayout*>(GetLayoutManager());
}

void BubbleDialogModelHost::ConfigureGridLayout() {
  auto* grid_layout = SetLayoutManager(std::make_unique<GridLayout>());
  LayoutProvider* const provider = LayoutProvider::Get();
  const int between_padding =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL);

  ColumnSet* const column_set = grid_layout->AddColumnSet(kColumnId);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER,
                        GridLayout::kFixedSize,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(GridLayout::kFixedSize, between_padding);

  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::ColumnSize::kFixed, 0, 0);
}

Textfield* BubbleDialogModelHost::AddOrUpdateTextfield(
    const ui::DialogModelTextfield& model) {
  // TODO(pbos): Handle updating existing field.
  DCHECK(!GetViewByID(model.model_field_id(GetPassKey())))
      << "BubbleDialogModelHost doesn't yet support updates to the model";

  auto textfield = std::make_unique<Textfield>();
  textfield->SetAccessibleName(
      model.accessible_name().empty() ? model.text() : model.accessible_name());
  textfield->SetText(model.text());

  property_changed_subscriptions_.push_back(
      textfield->AddTextChangedCallback(base::BindRepeating(
          &BubbleDialogModelHost::NotifyTextfieldTextChanged,
          base::Unretained(this), model.model_field_id(GetPassKey()),
          textfield.get())));

  auto* textfield_ptr = textfield.get();
  AddLabelAndField(model.label(), std::move(textfield),
                   textfield_ptr->GetFontList());

  return textfield_ptr;
}

Combobox* BubbleDialogModelHost::AddOrUpdateCombobox(
    ui::DialogModelCombobox* model) {
  // TODO(pbos): Handle updating existing field.
  DCHECK(!GetViewByID(model->model_field_id(GetPassKey())))
      << "BubbleDialogModelHost doesn't yet support updates to the model";

  auto combobox = std::make_unique<Combobox>(model->combobox_model());
  combobox->SetAccessibleName(model->accessible_name().empty()
                                  ? model->label()
                                  : model->accessible_name());
  combobox->set_listener(this);
  // TODO(pbos): Add subscription to combobox selected-index changes.
  combobox->SetSelectedIndex(model->selected_index());
  auto* combobox_ptr = combobox.get();
  AddLabelAndField(model->label(), std::move(combobox),
                   combobox_ptr->GetFontList());
  return combobox_ptr;
}

void BubbleDialogModelHost::AddLabelAndField(const base::string16& label_text,
                                             std::unique_ptr<View> field,
                                             const gfx::FontList& field_font) {
  constexpr int kFontContext = style::CONTEXT_LABEL;
  constexpr int kFontStyle = style::STYLE_PRIMARY;

  int row_height = LayoutProvider::GetControlHeightForFont(
      kFontContext, kFontStyle, field_font);
  GridLayout* const layout = GetGridLayout();
  layout->StartRow(GridLayout::kFixedSize, kColumnId, row_height);
  layout->AddView(
      std::make_unique<Label>(label_text, kFontContext, kFontStyle));
  layout->AddView(std::move(field));
}

void BubbleDialogModelHost::NotifyTextfieldTextChanged(int id,
                                                       Textfield* textfield) {
  model_->OnTextfieldTextChanged(GetPassKey(), id, textfield->GetText());
}

void BubbleDialogModelHost::NotifyComboboxSelectedIndexChanged(
    int id,
    Combobox* combobox) {
  model_->OnComboboxSelectedIndexChanged(GetPassKey(), id,
                                         combobox->GetSelectedIndex());
}

void BubbleDialogModelHost::ButtonPressed(Button* sender,
                                          const ui::Event& event) {
  model_->OnButtonPressed(GetPassKey(), sender->GetID(), event);
}

void BubbleDialogModelHost::OnPerformAction(Combobox* combobox) {
  // TODO(pbos): This should be a subscription through the Combobox directly,
  // but Combobox right now doesn't support listening to selected-index changes.
  NotifyComboboxSelectedIndexChanged(combobox->GetID(), combobox);

  model_->OnComboboxPerformAction(GetPassKey(), combobox->GetID());
}

void BubbleDialogModelHost::UpdateAccelerators() {
  // Dialog buttons can't be accessed before the widget is created. Delay until
  // ::OnDialogInitialized().
  if (!GetWidget())
    return;
  for (const auto& field : model_->fields(GetPassKey())) {
    if (field->accelerators(GetPassKey()).empty())
      continue;
    View* view = nullptr;
    if (field->model_field_id(GetPassKey()) == ui::DIALOG_BUTTON_OK) {
      view = GetOkButton();
    } else if (field->model_field_id(GetPassKey()) ==
               ui::DIALOG_BUTTON_CANCEL) {
      view = GetCancelButton();
    } else if (field.get() == model_->GetExtraButton()) {
      view = GetExtraView();
    } else {
      view = GetViewByID(field->model_field_id(GetPassKey()));
    }
    DCHECK(view);
    view->ResetAccelerators();
    for (const auto& accelerator : field->accelerators(GetPassKey()))
      view->AddAccelerator(accelerator);
  }
}

}  // namespace views
