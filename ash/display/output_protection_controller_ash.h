// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_OUTPUT_PROTECTION_CONTROLLER_ASH_H_
#define ASH_DISPLAY_OUTPUT_PROTECTION_CONTROLLER_ASH_H_

#include "ash/display/output_protection_delegate.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ui/display/manager/content_protection_manager.h"

namespace ash {

// Display content protection controller for running with ash.
class OutputProtectionControllerAsh
    : public OutputProtectionDelegate::Controller {
 public:
  OutputProtectionControllerAsh();
  ~OutputProtectionControllerAsh() override;

  // OutputProtectionDelegate::Controller:
  void QueryStatus(int64_t display_id, QueryStatusCallback callback) override;
  void SetProtection(int64_t display_id,
                     uint32_t protection_mask,
                     SetProtectionCallback callback) override;

 private:
  const display::ContentProtectionManager::ClientId client_id_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(OutputProtectionControllerAsh);
};

}  // namespace ash

#endif  // ASH_DISPLAY_OUTPUT_PROTECTION_CONTROLLER_ASH_H_
