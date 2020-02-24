// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantScreenContextModel {
 public:
  AssistantScreenContextModel();
  ~AssistantScreenContextModel();

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantScreenContextModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_H_
