// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model.h"

namespace ui {

DialogModelField::DialogModelField(util::PassKey<DialogModel>,
                                   DialogModel* model,
                                   Type type,
                                   int unique_id,
                                   base::flat_set<Accelerator> accelerators)
    : model_(model),
      type_(type),
      unique_id_(unique_id),
      accelerators_(std::move(accelerators)) {
  // TODO(pbos): Assert that unique_id_ is unique.
}

DialogModelField::~DialogModelField() = default;


DialogModelButton::Params::Params() = default;
DialogModelButton::Params::~Params() = default;

DialogModelButton::Params& DialogModelButton::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelButton::Params& DialogModelButton::Params::SetCallback(
    base::RepeatingCallback<void(const Event&)> callback) {
  callback_ = std::move(callback);
  return *this;
}

DialogModelButton::DialogModelButton(util::PassKey<DialogModel> pass_key,
                                     DialogModel* model,
                                     base::string16 label,
                                     const DialogModelButton::Params& params)
    : DialogModelField(pass_key,
                       model,
                       kButton,
                       params.unique_id_,
                       params.accelerators_),
      label_(std::move(label)),
      callback_(params.callback_) {}

DialogModelButton::~DialogModelButton() = default;

DialogModelCombobox::Params::Params() = default;
DialogModelCombobox::Params::~Params() = default;

DialogModelCombobox::Params& DialogModelCombobox::Params::SetUniqueId(
    int unique_id) {
  DCHECK_GE(unique_id, 0);
  unique_id_ = unique_id;
  return *this;
}

DialogModelCombobox::Params& DialogModelCombobox::Params::SetCallback(
    base::RepeatingClosure callback) {
  callback_ = std::move(callback);
  return *this;
}

DialogModelCombobox::Params& DialogModelCombobox::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelCombobox::Params& DialogModelCombobox::Params::SetAccessibleName(
    base::string16 accessible_name) {
  accessible_name_ = std::move(accessible_name);
  return *this;
}

DialogModelCombobox::DialogModelCombobox(
    util::PassKey<DialogModel> pass_key,
    DialogModel* model,
    base::string16 label,
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const DialogModelCombobox::Params& params)
    : DialogModelField(pass_key,
                       model,
                       kCombobox,
                       params.unique_id_,
                       params.accelerators_),
      label_(std::move(label)),
      accessible_name_(params.accessible_name_),
      selected_index_(combobox_model->GetDefaultIndex()),
      combobox_model_(std::move(combobox_model)),
      callback_(params.callback_) {}

DialogModelCombobox::~DialogModelCombobox() = default;

DialogModelTextfield::Params::Params() = default;
DialogModelTextfield::Params::~Params() = default;

DialogModelTextfield::Params& DialogModelTextfield::Params::SetUniqueId(
    int unique_id) {
  DCHECK_GE(unique_id, 0);
  unique_id_ = unique_id;
  return *this;
}

DialogModelTextfield::Params& DialogModelTextfield::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelTextfield::Params& DialogModelTextfield::Params::SetAccessibleName(
    base::string16 accessible_name) {
  accessible_name_ = accessible_name;
  return *this;
}

DialogModelTextfield::DialogModelTextfield(
    util::PassKey<DialogModel> pass_key,
    DialogModel* model,
    base::string16 label,
    base::string16 text,
    const ui::DialogModelTextfield::Params& params)
    : DialogModelField(pass_key,
                       model,
                       kTextfield,
                       params.unique_id_,
                       params.accelerators_),
      label_(label),
      accessible_name_(params.accessible_name_),
      text_(std::move(text)) {}

DialogModelTextfield::~DialogModelTextfield() = default;

}  // namespace ui