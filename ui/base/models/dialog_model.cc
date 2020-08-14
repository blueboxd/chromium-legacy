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
      GetPassKey(), this, std::move(label), std::move(text), params));
  if (host_)
    host_->OnFieldAdded(fields_.back().get());
}

void DialogModel::AddCombobox(base::string16 label,
                              std::unique_ptr<ui::ComboboxModel> combobox_model,
                              const DialogModelCombobox::Params& params) {
  fields_.push_back(std::make_unique<DialogModelCombobox>(
      GetPassKey(), this, std::move(label), std::move(combobox_model), params));
  if (host_)
    host_->OnFieldAdded(fields_.back().get());
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

void DialogModel::OnButtonPressed(util::PassKey<DialogModelHost>,
                                  DialogModelButton* button,
                                  const Event& event) {
  DCHECK_EQ(button->model_, this);
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
                                                 DialogModelCombobox* combobox,
                                                 int index) {
  DCHECK_EQ(combobox->model_, this);
  combobox->selected_index_ = index;
}

void DialogModel::OnComboboxPerformAction(util::PassKey<DialogModelHost>,
                                          DialogModelCombobox* combobox) {
  DCHECK_EQ(combobox->model_, this);
  if (combobox->callback_)
    combobox->callback_.Run();
}

void DialogModel::OnTextfieldTextChanged(util::PassKey<DialogModelHost>,
                                         DialogModelTextfield* textfield,
                                         base::string16 text) {
  DCHECK_EQ(textfield->model_, this);
  textfield->text_ = std::move(text);
}

void DialogModel::OnWindowClosing(util::PassKey<DialogModelHost>) {
  if (window_closing_callback_)
    std::move(window_closing_callback_).Run();
}

void DialogModel::AddDialogButton(int button_id,
                                  base::string16 label,
                                  const DialogModelButton::Params& params) {
  DCHECK(!host_);  // Dialog buttons should be added before adding to host.
  base::Optional<DialogModelButton>* button = nullptr;
  switch (button_id) {
    case ui::DIALOG_BUTTON_OK:
      button = &ok_button_;
      break;
    case ui::DIALOG_BUTTON_CANCEL:
      button = &cancel_button_;
      break;
    case kExtraButtonId:
      button = &extra_button_;
      break;
    default:
      NOTREACHED();
  }
  DCHECK(!button->has_value());
  button->emplace(GetPassKey(), this, std::move(label), params);
}

}  // namespace ui