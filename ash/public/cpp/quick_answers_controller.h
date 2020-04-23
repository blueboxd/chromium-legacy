// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_QUICK_ANSWERS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_QUICK_ANSWERS_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace quick_answers {
class QuickAnswersClient;
class QuickAnswersDelegate;
}  // namespace quick_answers
}  // namespace chromeos

namespace ash {

// A controller to manage quick answers UI.
class ASH_PUBLIC_EXPORT QuickAnswersController {
 public:
  QuickAnswersController();
  virtual ~QuickAnswersController();

  static QuickAnswersController* Get();

  // Passes in a client instance for the controller to use.
  virtual void SetClient(
      std::unique_ptr<chromeos::quick_answers::QuickAnswersClient> client) = 0;

  // Show the quick-answers view (and/or any accompanying/associated views like
  // user-consent view instead, if consent is not yet granted). |anchor_bounds|
  // is the bounds of the anchor view (which is the context menu for browser).
  // |title| is the text selected by the user.
  virtual void MaybeShowQuickAnswers(const gfx::Rect& anchor_bounds,
                                     const std::string& title) = 0;

  // Dismiss the quick-answers view (and/or any associated views like
  // user-consent view) currently shown. |is_active| is true if the quick-answer
  // result considered fulfilling a user's intent. For quick-answer rendered
  // along with browser context menu, if user didn't click on other context menu
  // items, it is considered as active impression.
  virtual void DismissQuickAnswers(bool is_active) = 0;

  // Update the bounds of the anchor view.
  virtual void UpdateQuickAnswersAnchorBounds(
      const gfx::Rect& anchor_bounds) = 0;

  virtual chromeos::quick_answers::QuickAnswersDelegate*
  GetQuickAnswersDelegate() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_QUICK_ANSWERS_CONTROLLER_H_
