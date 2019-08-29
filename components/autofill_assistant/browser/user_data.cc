// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/password_form.h"

namespace autofill_assistant {

LoginChoice::LoginChoice(const std::string& id,
                         const std::string& text,
                         int priority)
    : identifier(id), label(text), preselect_priority(priority) {}
LoginChoice::~LoginChoice() = default;

UserData::UserData() = default;
UserData::~UserData() = default;

CollectUserDataOptions::CollectUserDataOptions() = default;
CollectUserDataOptions::~CollectUserDataOptions() = default;

}  // namespace autofill_assistant
