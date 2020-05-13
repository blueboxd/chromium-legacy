// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ACCESS_CODE_INPUT_H_
#define ASH_LOGIN_UI_ACCESS_CODE_INPUT_H_

#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace ash {

class AccessCodeInput : public views::View, public views::TextfieldController {
 public:
  static constexpr int kAccessCodeInputFieldUnderlineThicknessDp = 2;
  static constexpr int kAccessCodeInputFieldHeightDp =
      24 + kAccessCodeInputFieldUnderlineThicknessDp;

  AccessCodeInput() = default;

  ~AccessCodeInput() override = default;

  // Deletes the last character.
  virtual void Backspace() = 0;

  // Appends a digit to the code.
  virtual void InsertDigit(int value) = 0;

  // Returns access code as string.
  virtual base::Optional<std::string> GetCode() const = 0;

  // Sets the color of the input text.
  virtual void SetInputColor(SkColor color) = 0;

  virtual void SetInputEnabled(bool input_enabled) = 0;

  // Clears the input field(s).
  virtual void ClearInput() = 0;
};

class FlexCodeInput : public AccessCodeInput {
 public:
  using OnInputChange = base::RepeatingCallback<void(bool enable_submit)>;
  using OnEnter = base::RepeatingClosure;
  using OnEscape = base::RepeatingClosure;

  // Builds the view for an access code that consists out of an unknown number
  // of digits. |on_input_change| will be called upon digit insertion, deletion
  // or change. |on_enter| will be called when code is complete and user presses
  // enter to submit it for validation. |on_escape| will be called when pressing
  // the escape key. |obscure_pin| determines whether the entered pin is
  // displayed as clear text or as bullet points.
  FlexCodeInput(OnInputChange on_input_change,
                OnEnter on_enter,
                OnEscape on_escape,
                bool obscure_pin);

  FlexCodeInput(const FlexCodeInput&) = delete;
  FlexCodeInput& operator=(const FlexCodeInput&) = delete;
  ~FlexCodeInput() override;

  // Appends |value| to the code
  void InsertDigit(int value) override;

  // Deletes the last character or the selected text.
  void Backspace() override;

  // Returns access code as string if field contains input.
  base::Optional<std::string> GetCode() const override;

  // Sets the color of the input text.
  void SetInputColor(SkColor color) override;

  void SetInputEnabled(bool input_enabled) override;

  // Clears text in input text field.
  void ClearInput() override;

  void RequestFocus() override;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // views::TextfieldController
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  views::Textfield* code_field_;

  // To be called when access input code changes (digit is inserted, deleted or
  // updated). Passes true when code non-empty.
  OnInputChange on_input_change_;

  // To be called when user pressed enter to submit.
  OnEnter on_enter_;

  // To be called when user presses escape to go back.
  OnEscape on_escape_;
};

// Accessible input field for a single digit in fixed length codes.
// Customizes field description and focus behavior.
class AccessibleInputField : public views::Textfield {
 public:
  AccessibleInputField() = default;
  ~AccessibleInputField() override = default;

  void set_accessible_description(const base::string16& description) {
    accessible_description_ = description;
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool IsGroupFocusTraversable() const override;
  View* GetSelectedViewForGroup(int group) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  base::string16 accessible_description_;

  DISALLOW_COPY_AND_ASSIGN(AccessibleInputField);
};

// Digital access code input view for variable length of input codes.
// Displays a separate underscored field for every input code digit.
class FixedLengthCodeInput : public AccessCodeInput {
 public:
  using OnInputChange =
      base::RepeatingCallback<void(bool last_field_active, bool complete)>;
  using OnEnter = base::RepeatingClosure;
  using OnEscape = base::RepeatingClosure;

  class TestApi {
   public:
    explicit TestApi(FixedLengthCodeInput* fixed_length_code_input)
        : fixed_length_code_input_(fixed_length_code_input) {}
    ~TestApi() = default;

    views::Textfield* GetInputTextField(int index) {
      DCHECK_LT(static_cast<size_t>(index),
                fixed_length_code_input_->input_fields_.size());
      return fixed_length_code_input_->input_fields_[index];
    }

   private:
    FixedLengthCodeInput* fixed_length_code_input_;
  };

  // Builds the view for an access code that consists out of |length| digits.
  // |on_input_change| will be called upon access code digit insertion, deletion
  // or change. True will be passed if the current code is complete (all digits
  // have input values) and false otherwise. |on_enter| will be called when code
  // is complete and user presses enter to submit it for validation. |on_escape|
  // will be called when pressing the escape key. |obscure_pin| determines
  // whether the entered pin is displayed as clear text or as bullet points.
  FixedLengthCodeInput(int length,
                       OnInputChange on_input_change,
                       OnEnter on_enter,
                       OnEscape on_escape,
                       bool obscure_pin);

  ~FixedLengthCodeInput() override;
  FixedLengthCodeInput(const FixedLengthCodeInput&) = delete;
  FixedLengthCodeInput& operator=(const FixedLengthCodeInput&) = delete;

  // Inserts |value| into the |active_field_| and moves focus to the next field
  // if it exists.
  void InsertDigit(int value) override;

  // Clears input from the |active_field_|. If |active_field| is empty moves
  // focus to the previous field (if exists) and clears input there.
  void Backspace() override;

  // Returns access code as string if all fields contain input.
  base::Optional<std::string> GetCode() const override;

  // Sets the color of the input text.
  void SetInputColor(SkColor color) override;

  // views::View:
  bool IsGroupFocusTraversable() const override;

  View* GetSelectedViewForGroup(int group) override;

  void RequestFocus() override;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;

  // Enables/disables entering a PIN. Currently, there is no use-case the uses
  // this with fixed length PINs.
  void SetInputEnabled(bool input_enabled) override;

  // Clears the PIN fields. Currently, there is no use-case the uses this with
  // fixed length PINs.
  void ClearInput() override;

 private:
  // Moves focus to the previous input field if it exists.
  void FocusPreviousField();

  // Moves focus to the next input field if it exists.
  void FocusNextField();

  // Returns whether last input field is currently active.
  bool IsLastFieldActive() const;

  // Returns pointer to the active input field.
  AccessibleInputField* ActiveField() const;

  // Returns text in the active input field.
  const base::string16& ActiveInput() const;

  // To be called when access input code changes (digit is inserted, deleted or
  // updated). Passes true when code is complete (all digits have input value)
  // and false otherwise.
  OnInputChange on_input_change_;

  // To be called when user pressed enter to submit.
  OnEnter on_enter_;
  // To be called when user pressed escape to close view.
  OnEscape on_escape_;

  // An active/focused input field index. Incoming digit will be inserted here.
  int active_input_index_ = 0;

  // Unowned input textfields ordered from the first to the last digit.
  std::vector<AccessibleInputField*> input_fields_;

  base::WeakPtrFactory<FixedLengthCodeInput> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_ACCESS_CODE_INPUT_H_