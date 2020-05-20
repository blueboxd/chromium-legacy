// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_HIERARCHY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_HIERARCHY_H_

#include "base/optional.h"
#include "chrome/browser/ui/webui/settings/chromeos/hierarchy.h"

namespace chromeos {
namespace settings {

// Fake Hierarchy implementation. Note that this class currently does not
// provide "alternate settings location" functionality.
class FakeHierarchy : public Hierarchy {
 public:
  FakeHierarchy();
  FakeHierarchy(const FakeHierarchy& other) = delete;
  FakeHierarchy& operator=(const FakeHierarchy& other) = delete;
  ~FakeHierarchy() override;

  void AddSubpageMetadata(
      int name_message_id,
      mojom::Section section,
      mojom::Subpage subpage,
      base::Optional<mojom::Subpage> parent_subpage = base::nullopt);
  void AddSettingMetadata(
      mojom::Section section,
      mojom::Setting setting,
      base::Optional<mojom::Subpage> parent_subpage = base::nullopt);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_HIERARCHY_H_
