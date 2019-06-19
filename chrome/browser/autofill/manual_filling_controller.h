// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image.h"

// Controller interface for the view that includes the keyboard accessory and
// the accessory sheet below it. Implementations of this interface create and
// own a ManualFillingViewInterface.
//
// The manual filling controller forwards requests from type-specific accessory
// controllers (Passwords and Autofill) to the view. The view notifies this
// controller about interactions (such as requesting to fill a password
// suggestion) and forwards the request to the corresponding type-specific
// accessory controller.
//
// This controller also implements the logic to show/hide the keyboard
// accessory.
//
// ManualFillingController::GetOrCreate() should be used
// by type-specific controllers to obtain an instance of this class for a given
// WebContents. There is only one instance per WebContents, which is created the
// first time |GetOrCreate()| is invoked.
//
// Usage example:
//   auto controller = ManualFillingController::GetOrCreate(web_contents);
//   DCHECK(controller);
//   controller->RefreshSuggestionsForField(...);
class ManualFillingController {
 public:
  // The controller checks if at least one of these sources needs the accessory
  // to be displayed.
  enum class FillingSource { AUTOFILL, PASSWORD_FALLBACKS, ADDRESS_FALLBACKS };

  ManualFillingController() = default;
  virtual ~ManualFillingController() = default;

  // Returns a weak pointer to the unique ManualFillingController instance
  // associated with a WebContents. The first invocation creates an instance
  // and attaches it to the WebContents; the same instance is returned by all
  // future invocations for the same WebContents.
  static base::WeakPtr<ManualFillingController> GetOrCreate(
      content::WebContents* contents);

  // --------------------------------------------
  // Methods called by type-specific controllers.
  // --------------------------------------------

  // Forwards |accessory_sheet_data| to the view provided by a type-specific
  // controller to be shown on the accessory sheet.
  virtual void RefreshSuggestionsForField(
      autofill::mojom::FocusedFieldType focused_field_type,
      const autofill::AccessorySheetData& accessory_sheet_data) = 0;

  // Completes a filling attempt by recording metrics, giving feedback to the
  // user and dismissing the accessory sheet.
  virtual void OnFilledIntoFocusedField(
      autofill::mojom::FillingStatus status) = 0;

  // Requests to show the accessory bar. The accessory will only be shown
  // when the keyboard becomes visible.
  // TODO(crbug.com/965478): Rename method to reflect latest logic changes.
  virtual void ShowWhenKeyboardIsVisible(FillingSource source) = 0;

  // Requests to show the touch to fill sheet.
  virtual void ShowTouchToFillSheet(
      const autofill::AccessorySheetData& data) = 0;

  // This signals that the accessory data corresponding to |source| doesn't
  // need to be shown anymore. If no FillingSource needs to be shown, the
  // accessory sheet and bar will be hidden.
  virtual void DeactivateFillingSource(FillingSource source) = 0;

  // Requests to hide the accessory. This hides both the accessory sheet
  // (if open) and the accessory bar.
  virtual void Hide() = 0;

  // Notifies the view that automatic password generation status changed.
  //
  // TODO(crbug.com/905669): This controller doesn't need to know about password
  // generation. Generalize this to send to the UI the information that an
  // action (given by an enum param) is available.
  virtual void OnAutomaticGenerationStatusChanged(bool available) = 0;

  // --------------------------
  // Methods called by UI code:
  // --------------------------

  // Called by the UI code to request that |text_to_fill| is to be filled into
  // the currently focused field. Forwards the request to a type-specific
  // accessory controller.
  virtual void OnFillingTriggered(
      autofill::AccessoryTabType type,
      const autofill::UserInfo::Field& selection) = 0;

  // Called by the UI code because a user triggered the |selected_action|,
  // such as "Manage passwords...".
  virtual void OnOptionSelected(
      autofill::AccessoryAction selected_action) const = 0;

  // Gets an icon for the currently focused frame and passes it to
  // |icon_callback|. The callback is invoked with an image unless an icon for
  // a new origin was called. In the latter case, the callback is dropped.
  // The callback is called with an |IsEmpty()| image if there is no favicon.
  //
  // TODO(crbug.com/905669): This controller doesn't need to know about
  // favicons. Generalize this to forward an icon requests from the UI to a
  // type-specific accessory controller.
  virtual void GetFavicon(
      int desired_size_in_pixel,
      base::OnceCallback<void(const gfx::Image&)> icon_callback) = 0;

  // -----------------
  // Member accessors:
  // -----------------

  // The web page view containing the focused field.
  virtual gfx::NativeView container_view() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManualFillingController);
};

#endif  // CHROME_BROWSER_AUTOFILL_MANUAL_FILLING_CONTROLLER_H_
