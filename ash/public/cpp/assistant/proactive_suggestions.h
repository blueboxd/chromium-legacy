// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_H_
#define ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// A data structure to represent a renderable set of proactive suggestions.
struct ASH_PUBLIC_EXPORT ProactiveSuggestions {
  const std::string description;
  const std::string html;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_H_
