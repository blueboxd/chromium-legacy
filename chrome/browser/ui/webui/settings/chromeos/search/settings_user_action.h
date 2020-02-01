// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SETTINGS_USER_ACTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SETTINGS_USER_ACTION_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"

namespace chromeos {
namespace settings {

// An action taken by a user in Chrome OS settings.
class SettingsUserAction {
 public:
  enum class Type {
    kClick,
    kNavigation,
    kVisibilityChange,
    kSearch,
    kSettingChange
  };

  SettingsUserAction(const SettingsUserAction& other);
  SettingsUserAction& operator=(const SettingsUserAction& other);

  ~SettingsUserAction();

  Type type() const { return type_; }
  base::Time timestamp() const { return timestamp_; }

 protected:
  explicit SettingsUserAction(Type type);

 private:
  Type type_;
  base::Time timestamp_ = base::Time::Now();
};

// A click action taken by a user in Chrome OS settings.
class SettingsClickAction : public SettingsUserAction {
 public:
  SettingsClickAction();
  SettingsClickAction(const SettingsClickAction& other);
  SettingsClickAction& operator=(const SettingsClickAction& other);
  ~SettingsClickAction();
};

// A navigation action taken by a user in Chrome OS settings.
class SettingsNavigationAction : public SettingsUserAction {
 public:
  explicit SettingsNavigationAction(mojom::SettingsSectionName section_name);
  SettingsNavigationAction(const SettingsNavigationAction& other);
  SettingsNavigationAction& operator=(const SettingsNavigationAction& other);
  ~SettingsNavigationAction();

  mojom::SettingsSectionName section_name() const { return section_name_; }

 private:
  mojom::SettingsSectionName section_name_;
};

// A visibility change action taken by a user in Chrome OS settings.
class SettingsVisibilityChangeAction : public SettingsUserAction {
 public:
  explicit SettingsVisibilityChangeAction(
      mojom::SettingsPageVisibility page_visibility);
  SettingsVisibilityChangeAction(const SettingsVisibilityChangeAction& other);
  SettingsVisibilityChangeAction& operator=(
      const SettingsVisibilityChangeAction& other);
  ~SettingsVisibilityChangeAction();

  mojom::SettingsPageVisibility page_visibility() const {
    return page_visibility_;
  }

 private:
  mojom::SettingsPageVisibility page_visibility_;
};

// A search action taken by a user in Chrome OS settings.
class SettingsSearchAction : public SettingsUserAction {
 public:
  explicit SettingsSearchAction(const std::string& search_query);
  SettingsSearchAction(const SettingsSearchAction& other);
  SettingsSearchAction& operator=(const SettingsSearchAction& other);
  ~SettingsSearchAction();

  const std::string& search_query() const { return search_query_; }

 private:
  std::string search_query_;
};

// A settings change action taken by a user in Chrome OS settings.
class SettingsChangeAction : public SettingsUserAction {
 public:
  SettingsChangeAction();
  SettingsChangeAction(const SettingsChangeAction& other);
  SettingsChangeAction& operator=(const SettingsChangeAction& other);
  ~SettingsChangeAction();
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SETTINGS_USER_ACTION_H_
