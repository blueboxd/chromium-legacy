// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "components/autofill/core/browser/ui/accessory_sheet_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace password_manager {
class ContentPasswordManagerDriver;
}  // namespace password_manager

class ManualFillingController;

// Encapsulates the data needed from the password manager backend to
// eventually render a username/password in the manual filling frontend.
// TODO(crbug.com/941654): It's probably cleanest to use PasswordForm directly
// instead of PasswordAccessorySuggestion as the type for the
// AccessoryController.
struct PasswordAccessorySuggestion {
  PasswordAccessorySuggestion(base::string16 password,
                              base::string16 username,
                              bool username_selectable)
      : password(std::move(password)),
        username(std::move(username)),
        username_selectable(username_selectable) {}

  base::string16 password;
  base::string16 username;
  bool username_selectable;
};

// Use either PasswordAccessoryController::GetOrCreate or
// PasswordAccessoryController::GetIfExisting to obtain instances of this class.
// This class exists for every tab and should never store state based on the
// contents of one of its frames. This can cause cross-origin hazards.
class PasswordAccessoryControllerImpl
    : public PasswordAccessoryController,
      public content::WebContentsUserData<PasswordAccessoryControllerImpl> {
 public:
  ~PasswordAccessoryControllerImpl() override;

  // AccessoryController:
  void OnFillingTriggered(const autofill::UserInfo::Field& selection) override;
  void OnOptionSelected(autofill::AccessoryAction selected_action) override;

  // PasswordAccessoryController:
  void SavePasswordsForOrigin(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const url::Origin& origin) override;
  void OnFilledIntoFocusedField(autofill::mojom::FillingStatus status) override;
  void RefreshSuggestionsForField(
      autofill::mojom::FocusedFieldType focused_field_type,
      bool is_manual_generation_available) override;
  void OnGenerationRequested(bool manual) override;
  void DidNavigateMainFrame() override;
  void GetFavicon(
      int desired_size_in_pixel,
      base::OnceCallback<void(const gfx::Image&)> icon_callback) override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a manual filling
  // controller.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      favicon::FaviconService* favicon_service);

  // True if the focus event was sent for the current focused frame or if it is
  // a blur event and no frame is focused. This check avoids reacting to
  // obsolete events that arrived in an unexpected order.
  // TODO(crbug.com/968162): Introduce the concept of active frame to the
  // accessory controller and move this check in the controller.
  static bool ShouldAcceptFocusEvent(
      content::WebContents* web_contents,
      password_manager::ContentPasswordManagerDriver* driver,
      autofill::mojom::FocusedFieldType focused_field_type);

 private:
  // Data allowing to cache favicons and favicon-related requests.
  struct FaviconRequestData;

  friend class content::WebContentsUserData<PasswordAccessoryControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit PasswordAccessoryControllerImpl(content::WebContents* contents);

  // Constructor that allows to inject a mock or fake view.
  PasswordAccessoryControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller,
      favicon::FaviconService* favicon_service);

  // Handles a favicon response requested by |GetFavicon| and responds to
  // pending favicon requests with a (possibly empty) icon bitmap.
  void OnImageFetched(
      url::Origin origin,
      const favicon_base::FaviconRawBitmapResult& bitmap_results);

  // Returns true if |suggestion| matches a credential for |origin|.
  bool AppearsInSuggestions(const base::string16& suggestion,
                            bool is_password,
                            const url::Origin& origin) const;

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization allows injecting mocks for tests.
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  url::Origin GetFocusedFrameOrigin() const;
  std::vector<PasswordAccessorySuggestion> GetSuggestions() const;

  // ------------------------------------------------------------------------
  // Members - Make sure to NEVER store state related to a single frame here!
  // ------------------------------------------------------------------------

  // Contains the last set of credentials by origin.
  std::map<url::Origin, std::vector<PasswordAccessorySuggestion>>
      origin_suggestions_;

  // The tab for which this class is scoped.
  content::WebContents* web_contents_;

  // TODO(fhorschig): Find a way to use unordered_map with origin keys.
  // A cache for all favicons that were requested. This includes all iframes
  // for which the accessory was displayed.
  std::map<url::Origin, FaviconRequestData> icons_request_data_;

  // Used to track a requested favicon. If the set of suggestion changes, this
  // object aborts the request. Upon destruction, requests are cancelled, too.
  base::CancelableTaskTracker favicon_tracker_;

  // The password accessory controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  // The favicon service used to make retrieve icons for a given origin.
  favicon::FaviconService* favicon_service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryControllerImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_ACCESSORY_CONTROLLER_IMPL_H_
