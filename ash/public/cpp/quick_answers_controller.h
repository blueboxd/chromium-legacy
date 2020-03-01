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

  // Initiate the quick answers view. |anchor_bounds| is the bounds of anchor
  // view which is context menu. |title| is the text selected by the user.
  virtual void CreateQuickAnswersView(const gfx::Rect& anchor_bounds,
                                      const std::string& title) = 0;

  // Dismiss the quick answers view.
  virtual void DismissQuickAnswersView() = 0;

  virtual chromeos::quick_answers::QuickAnswersDelegate*
  GetQuickAnswersDelegate() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_QUICK_ANSWERS_CONTROLLER_H_
