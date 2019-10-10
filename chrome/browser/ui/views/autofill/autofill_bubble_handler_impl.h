// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

class Profile;
class ToolbarButtonProvider;

namespace content {
class WebContents;
}

namespace autofill {
class LocalCardMigrationBubble;
class LocalCardMigrationBubbleController;
class SaveCardBubbleView;
class SaveCardBubbleController;

class AutofillBubbleHandlerImpl : public AutofillBubbleHandler,
                                  public PersonalDataManagerObserver {
 public:
  AutofillBubbleHandlerImpl(ToolbarButtonProvider* toolbar_button_provider,
                            Profile* profile);
  ~AutofillBubbleHandlerImpl() override;

  // AutofillBubbleHandler:
  SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_user_gesture) override;
  LocalCardMigrationBubble* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;
  void OnPasswordSaved() override;

  // autofill::PersonalDataManagerObserver:
  void OnCreditCardSaved() override;

 private:
  // Executes highlight animation on toolbar's avatar icon.
  void ShowAvatarHighlightAnimation();

  ToolbarButtonProvider* toolbar_button_provider_ = nullptr;

  ScopedObserver<autofill::PersonalDataManager,
                 autofill::PersonalDataManagerObserver>
      personal_data_manager_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillBubbleHandlerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
