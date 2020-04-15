// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// The interface for the Assistant controller.
class ASH_PUBLIC_EXPORT AssistantController {
 public:
  static AssistantController* Get();

 protected:
  AssistantController();
  virtual ~AssistantController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_
