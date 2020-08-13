// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model.h"

namespace ui {

DialogModel::Builder::Builder(std::unique_ptr<DialogModelDelegate> delegate)
    : model_(std::make_unique<DialogModel>(util::PassKey<Builder>(),
                                           std::move(delegate))) {}
DialogModel::Builder::~Builder() {
  DCHECK(!model_) << "Model should've been built.";
}

std::unique_ptr<DialogModel> DialogModel::Builder::Build() {
  DCHECK(model_);
  return std::move(model_);
}

DialogModel::Builder& DialogModel::Builder::SetShowCloseButton(
    bool show_close_button) {
  model_->show_close_button_ = show_close_button;
  return *this;
}

DialogModel::Builder& DialogModel::Builder::SetTitle(base::string16 title) {
  model_->title_ = std::move(title);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::SetCloseCallback(
    base::OnceClosure callback) {
  model_->close_callback_ = std::move(callback);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::SetWindowClosingCallback(
    base::OnceClosure callback) {
  model_->window_closing_callback_ = std::move(callback);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddOkButton(
    base::OnceClosure callback,
    base::string16 label,
    const DialogModelButton::Params& params) {
  DCHECK(!params.has_callback()) << "Use |callback| only.";
  DCHECK(!model_->accept_callback_);
  model_->accept_callback_ = std::move(callback);
  model_->AddDialogButton(ui::DIALOG_BUTTON_OK, std::move(label), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddCancelButton(
    base::OnceClosure callback,
    base::string16 label,
    const DialogModelButton::Params& params) {
  DCHECK(!params.has_callback()) << "Use |callback| only.";
  DCHECK(!model_->cancel_callback_);
  model_->cancel_callback_ = std::move(callback);
  model_->AddDialogButton(ui::DIALOG_BUTTON_CANCEL, std::move(label), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddDialogExtraButton(
    base::string16 label,
    const DialogModelButton::Params& params) {
  model_->AddDialogButton(kExtraButtonId, std::move(label), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddTextfield(
    base::string16 label,
    base::string16 text,
    const DialogModelTextfield::Params& params) {
  model_->AddTextfield(std::move(label), std::move(text), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddCombobox(
    base::string16 label,
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const DialogModelCombobox::Params& params) {
  model_->AddCombobox(std::move(label), std::move(combobox_model), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::SetInitiallyFocusedField(
    int unique_id) {
  // This must be called with unique_id >= 0 (-1 is "no ID").
  DCHECK_GE(unique_id, 0);
  // This can only be called once.
  DCHECK(!model_->initially_focused_field_);
  model_->initially_focused_field_ = unique_id;
  return *this;
}

DialogModel::DialogModel(util::PassKey<Builder>,
                         std::unique_ptr<DialogModelDelegate> delegate)
    : delegate_(std::move(delegate)) {
  delegate_->set_dialog_model(this);
}

DialogModel::~DialogModel() = default;

void DialogModel::AddTextfield(base::string16 label,
                               base::string16 text,
                               const DialogModelTextfield::Params& params) {
  fields_.push_back(std::make_unique<DialogModelTextfield>(
      ReserveField(), std::move(label), std::move(text), params));
  if (host_)
    host_->OnModelChanged(this);
}

void DialogModel::AddCombobox(base::string16 label,
                              std::unique_ptr<ui::ComboboxModel> combobox_model,
                              const DialogModelCombobox::Params& params) {
  fields_.push_back(std::make_unique<DialogModelCombobox>(
      ReserveField(), std::move(label), std::move(combobox_model), params));
  if (host_)
    host_->OnModelChanged(this);
}

DialogModelField* DialogModel::GetFieldByUniqueId(int unique_id) {
  for (auto& field : fields_) {
    if (field->unique_id_ == unique_id)
      return field.get();
  }
  NOTREACHED();
  return nullptr;
}

DialogModelButton* DialogModel::GetButtonByUniqueId(int unique_id) {
  auto* field = GetFieldByUniqueId(unique_id);
  DCHECK_EQ(field->type_, DialogModelField::kButton);
  return static_cast<DialogModelButton*>(field);
}

DialogModelCombobox* DialogModel::GetComboboxByUniqueId(int unique_id) {
  auto* field = GetFieldByUniqueId(unique_id);

  DCHECK_EQ(field->type_, DialogModelField::kCombobox);
  return static_cast<DialogModelCombobox*>(field);
}

DialogModelTextfield* DialogModel::GetTextfieldByUniqueId(int unique_id) {
  auto* field = GetFieldByUniqueId(unique_id);
  DCHECK_EQ(field->type_, DialogModelField::kTextfield);
  return static_cast<DialogModelTextfield*>(field);
}

DialogModelButton* DialogModel::GetDialogButton(DialogButton button) {
  return GetButtonFromModelFieldId(button);
}

DialogModelButton* DialogModel::GetExtraButton() {
  return GetButtonFromModelFieldId(kExtraButtonId);
}

void DialogModel::OnButtonPressed(util::PassKey<DialogModelHost>,
                                  int id,
                                  const Event& event) {
  DCHECK_GT(id, DIALOG_BUTTON_LAST);

  auto* button = GetButtonFromModelFieldId(id);
  if (button->callback_)
    button->callback_.Run(event);
}

void DialogModel::OnDialogAccepted(util::PassKey<DialogModelHost>) {
  if (accept_callback_)
    std::move(accept_callback_).Run();
}

void DialogModel::OnDialogCancelled(util::PassKey<DialogModelHost>) {
  if (cancel_callback_)
    std::move(cancel_callback_).Run();
}

void DialogModel::OnDialogClosed(util::PassKey<DialogModelHost>) {
  if (close_callback_)
    std::move(close_callback_).Run();
}

void DialogModel::OnComboboxSelectedIndexChanged(util::PassKey<DialogModelHost>,
                                                 int id,
                                                 int index) {
  GetComboboxFromModelFieldId(id)->selected_index_ = index;
}

void DialogModel::OnComboboxPerformAction(util::PassKey<DialogModelHost>,
                                          int id) {
  auto* model = GetComboboxFromModelFieldId(id);
  if (model->callback_)
    model->callback_.Run();
}

void DialogModel::OnTextfieldTextChanged(util::PassKey<DialogModelHost>,
                                         int id,
                                         base::string16 text) {
  GetTextfieldFromModelFieldId(id)->text_ = text;
}

void DialogModel::OnWindowClosing(util::PassKey<DialogModelHost>) {
  if (window_closing_callback_)
    std::move(window_closing_callback_).Run();
}

void DialogModel::AddDialogButton(int button,
                                  base::string16 label,
                                  const DialogModelButton::Params& params) {
  DCHECK_LE(button, kExtraButtonId);
  if (button != kExtraButtonId)  // Dialog buttons should use dialog callbacks.
    DCHECK(!params.has_callback());
  DCHECK(!host_);  // Dialog buttons should be added before adding to host.
  DCHECK(!GetFieldFromModelFieldId(button));
  fields_.push_back(std::make_unique<DialogModelButton>(
      DialogModelField::Reservation(this, button), std::move(label), params));
}

DialogModelField* DialogModel::GetFieldFromModelFieldId(int id) {
  for (const auto& field : fields_) {
    if (id == field->model_field_id_)
      return field.get();
  }
  return nullptr;
}

DialogModelButton* DialogModel::GetButtonFromModelFieldId(int id) {
  auto* field = GetFieldFromModelFieldId(id);
  DCHECK(field);
  DCHECK_EQ(field->type_, DialogModelField::kButton);
  return static_cast<DialogModelButton*>(field);
}

DialogModelCombobox* DialogModel::GetComboboxFromModelFieldId(int id) {
  auto* field = GetFieldFromModelFieldId(id);
  DCHECK(field);
  DCHECK_EQ(field->type_, DialogModelField::kCombobox);
  return static_cast<DialogModelCombobox*>(field);
}

DialogModelTextfield* DialogModel::GetTextfieldFromModelFieldId(int id) {
  auto* field = GetFieldFromModelFieldId(id);
  DCHECK(field);
  DCHECK_EQ(field->type_, DialogModelField::kTextfield);
  return static_cast<DialogModelTextfield*>(field);
}

DialogModelField::Reservation DialogModel::ReserveField() {
  const int id = next_field_id_++;
  DCHECK(!GetFieldFromModelFieldId(id));

  return DialogModelField::Reservation(this, id);
}

}  // namespace ui