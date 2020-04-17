// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/weak_ptr.h"

class GURL;

namespace ash {

// The interface for the Assistant controller.
class ASH_PUBLIC_EXPORT AssistantController {
 public:
  // Returns the singleton instance owned by Shell.
  static AssistantController* Get();

  // Opens the specified |url| in a new browser tab. Special handling is applied
  // to deep links which may cause deviation from this behavior.
  virtual void OpenUrl(const GURL& url,
                       bool in_background = false,
                       bool from_server = false) = 0;

  // Returns a weak pointer to this instance.
  virtual base::WeakPtr<AssistantController> GetWeakPtr() = 0;

 protected:
  AssistantController();
  virtual ~AssistantController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_CONTROLLER_ASSISTANT_CONTROLLER_H_
