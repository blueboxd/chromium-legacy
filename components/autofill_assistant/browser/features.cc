// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/features.h"

#include "base/feature_list.h"

namespace autofill_assistant {
namespace features {

const base::Feature kAutofillAssistant{"AutofillAssistant",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Guard for the end condition when a non-renderer-initiated navigation occurs
// while the AutofillAssistant is in RUNNING state.
// TODO(b/159309621): Remove this if the end condition shows no unwanted side
// effects.
const base::Feature kAutofillAssistantBreakOnRunningNavigation{
    "AutofillAssistantBreakOnRunningNavigation",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable Assistant Autofill in a normal Chrome tab.
const base::Feature kAutofillAssistantChromeEntry{
    "AutofillAssistantChromeEntry", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillAssistantDirectActions{
    "AutofillAssistantDirectActions", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace autofill_assistant
