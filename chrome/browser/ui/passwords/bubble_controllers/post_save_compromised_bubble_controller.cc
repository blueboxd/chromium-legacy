// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/post_save_compromised_bubble_controller.h"

PostSaveCompromisedBubbleController::PostSaveCompromisedBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          password_manager::metrics_util::
              AUTOMATIC_COMPROMISED_CREDENTIALS_REMINDER) {}

PostSaveCompromisedBubbleController::~PostSaveCompromisedBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  if (!interaction_reported_)
    OnBubbleClosing();
}

base::string16 PostSaveCompromisedBubbleController::GetTitle() const {
  return base::string16();
}

void PostSaveCompromisedBubbleController::ReportInteractions() {}
