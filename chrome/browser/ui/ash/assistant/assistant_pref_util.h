// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_PREF_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_PREF_UTIL_H_

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;

namespace assistant {
namespace prefs {

extern const char kAssistantDisabledByPolicy[];

// Registers Assistant specific profile preferences.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace assistant

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_PREF_UTIL_H_
