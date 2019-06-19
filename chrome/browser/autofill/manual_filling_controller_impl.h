// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_view_interface.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class AddressAccessoryController;
}
class AccessoryController;
class PasswordAccessoryController;
class TouchToFillController;

// Use ManualFillingController::GetOrCreate to obtain instances of this class.
class ManualFillingControllerImpl
    : public ManualFillingController,
      public content::WebContentsUserData<ManualFillingControllerImpl> {
 public:
  ~ManualFillingControllerImpl() override;

  // ManualFillingController:
  void RefreshSuggestionsForField(
      autofill::mojom::FocusedFieldType focused_field_type,
      const autofill::AccessorySheetData& accessory_sheet_data) override;
  void OnFilledIntoFocusedField(autofill::mojom::FillingStatus status) override;
  void ShowWhenKeyboardIsVisible(FillingSource source) override;
  void ShowTouchToFillSheet(const autofill::AccessorySheetData& data) override;
  void DeactivateFillingSource(FillingSource source) override;
  void Hide() override;
  void OnAutomaticGenerationStatusChanged(bool available) override;
  void OnFillingTriggered(autofill::AccessoryTabType type,
                          const autofill::UserInfo::Field& selection) override;
  void OnOptionSelected(
      autofill::AccessoryAction selected_action) const override;
  void GetFavicon(
      int desired_size_in_pixel,
      base::OnceCallback<void(const gfx::Image&)> icon_callback) override;
  gfx::NativeView container_view() const override;

  // Returns a weak pointer for this object.
  base::WeakPtr<ManualFillingController> AsWeakPtr();

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows injecting a fake/mock
  // view and type-specific controllers.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<PasswordAccessoryController> pwd_controller,
      base::WeakPtr<autofill::AddressAccessoryController> address_controller,
      std::unique_ptr<ManualFillingViewInterface> test_view);

#if defined(UNIT_TEST)
  // Returns the held view for testing.
  ManualFillingViewInterface* view() const { return view_.get(); }
#endif  // defined(UNIT_TEST)
  // Returns the connected password accessory controller for testing.
  PasswordAccessoryController* password_controller_for_testing() const {
    return pwd_controller_.get();
  }

 protected:
  friend class ManualFillingController;  // Allow protected access in factories.

  // Enables calling initialization code that relies on a fully constructed
  // ManualFillingController that is attached to a WebContents instance.
  // This is matters for subcomponents which lazily trigger the creation of this
  // class. If called in constructors, it would cause an infinite creation loop.
  void Initialize();

 private:
  friend class content::WebContentsUserData<ManualFillingControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit ManualFillingControllerImpl(content::WebContents* contents);

  // Constructor that allows to inject a mock or fake view.
  ManualFillingControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<PasswordAccessoryController> pwd_controller,
      base::WeakPtr<autofill::AddressAccessoryController> address_controller,
      std::unique_ptr<ManualFillingViewInterface> view);

  // Returns the controller that is responsible for a tab of given |type|.
  AccessoryController* GetControllerForTab(autofill::AccessoryTabType type);

  // Returns the controller that is responsible for a given |action|.
  AccessoryController* GetControllerForAction(
      autofill::AccessoryAction action) const;

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // This set contains sources to be shown to the user.
  base::flat_set<FillingSource> visible_sources_;

  // The password accessory controller object to forward view requests to.
  base::WeakPtr<PasswordAccessoryController> pwd_controller_;

  // The address accessory controller object to forward view requests to.
  base::WeakPtr<autofill::AddressAccessoryController> address_controller_;

  // The touch to fill controller object to forward view requests to.
  base::WeakPtr<TouchToFillController> touch_to_fill_controller_;

  // Hold the native instance of the view. Must be last declared and initialized
  // member so the view can be created in the constructor with a fully set up
  // controller instance.
  std::unique_ptr<ManualFillingViewInterface> view_ =
      ManualFillingViewInterface::Create(this);

  base::WeakPtrFactory<ManualFillingControllerImpl> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ManualFillingControllerImpl);
};

#endif  // CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_IMPL_H_
