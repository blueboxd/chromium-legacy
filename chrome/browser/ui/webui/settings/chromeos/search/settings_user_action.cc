// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/settings_user_action.h"

#include "base/logging.h"

namespace chromeos {
namespace settings {

// SettingsUserAction:

SettingsUserAction::SettingsUserAction(Type type) : type_(type) {}

SettingsUserAction::SettingsUserAction(const SettingsUserAction& other) =
    default;

SettingsUserAction& SettingsUserAction::operator=(
    const SettingsUserAction& other) = default;

SettingsUserAction::~SettingsUserAction() = default;

// SettingsClickAction:

SettingsClickAction::SettingsClickAction() : SettingsUserAction(Type::kClick) {}

SettingsClickAction::SettingsClickAction(const SettingsClickAction& other) =
    default;

SettingsClickAction& SettingsClickAction::operator=(
    const SettingsClickAction& other) = default;

SettingsClickAction::~SettingsClickAction() = default;

// SettingsNavigationAction:

SettingsNavigationAction::SettingsNavigationAction(
    mojom::SettingsSectionName section_name)
    : SettingsUserAction(Type::kNavigation), section_name_(section_name) {}

SettingsNavigationAction::SettingsNavigationAction(
    const SettingsNavigationAction& other) = default;

SettingsNavigationAction& SettingsNavigationAction::operator=(
    const SettingsNavigationAction& other) = default;

SettingsNavigationAction::~SettingsNavigationAction() = default;

// SettingsVisibilityChangeAction:

SettingsVisibilityChangeAction::SettingsVisibilityChangeAction(
    mojom::SettingsPageVisibility page_visibility)
    : SettingsUserAction(Type::kVisibilityChange),
      page_visibility_(page_visibility) {}

SettingsVisibilityChangeAction::SettingsVisibilityChangeAction(
    const SettingsVisibilityChangeAction& other) = default;

SettingsVisibilityChangeAction& SettingsVisibilityChangeAction::operator=(
    const SettingsVisibilityChangeAction& other) = default;

SettingsVisibilityChangeAction::~SettingsVisibilityChangeAction() = default;

// SettingsSearchAction:

SettingsSearchAction::SettingsSearchAction(const std::string& search_query)
    : SettingsUserAction(Type::kSearch), search_query_(search_query) {}

SettingsSearchAction::SettingsSearchAction(const SettingsSearchAction& other) =
    default;

SettingsSearchAction& SettingsSearchAction::operator=(
    const SettingsSearchAction& other) = default;

SettingsSearchAction::~SettingsSearchAction() = default;

// SettingsChangeAction:

SettingsChangeAction::SettingsChangeAction()
    : SettingsUserAction(Type::kSettingChange) {}

SettingsChangeAction::SettingsChangeAction(const SettingsChangeAction& other) =
    default;

SettingsChangeAction& SettingsChangeAction::operator=(
    const SettingsChangeAction& other) = default;

SettingsChangeAction::~SettingsChangeAction() = default;

}  // namespace settings
}  // namespace chromeos
