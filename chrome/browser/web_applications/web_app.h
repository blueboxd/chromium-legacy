// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include <bitset>
#include <iosfwd>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace web_app {

class WebApp {
 public:
  explicit WebApp(const AppId& app_id);
  ~WebApp();

  const AppId& app_id() const { return app_id_; }

  const std::string& name() const { return name_; }
  const std::string& description() const { return description_; }
  const GURL& launch_url() const { return launch_url_; }
  const GURL& scope() const { return scope_; }
  const base::Optional<SkColor>& theme_color() const { return theme_color_; }
  blink::mojom::DisplayMode display_mode() const { return display_mode_; }
  bool is_locally_installed() const { return is_locally_installed_; }
  // Sync-initiated installation produces a sync placeholder app awaiting for
  // full installation process. The sync placeholder app has only app_id,
  // launch_url and sync_data fields defined, no icons. If online install
  // succeeds, icons get downloaded and all the fields get their values. If
  // install fails, icons get generated using |sync_data| fields.
  bool is_sync_placeholder() const { return is_sync_placeholder_; }

  struct IconInfo {
    GURL url;
    int size_in_px;
  };
  using Icons = std::vector<IconInfo>;
  const Icons& icons() const { return icons_; }

  // While local |name| and |theme_color| may vary from device to device, the
  // synced copies of these fields are replicated to all devices. The synced
  // copies are read by a device to generate a placeholder icon (if needed). Any
  // device may write new values to |sync_data|, random last update wins.
  struct SyncData {
    SyncData();
    ~SyncData();
    std::string name;
    base::Optional<SkColor> theme_color;
  };
  const SyncData& sync_data() const { return sync_data_; }

  // A Web App can be installed from multiple sources simultaneously. Installs
  // add a source to the app. Uninstalls remove a source from the app.
  void AddSource(Source::Type source);
  void RemoveSource(Source::Type source);
  bool HasAnySources() const;

  bool IsSynced() const;

  void SetName(const std::string& name);
  void SetDescription(const std::string& description);
  void SetLaunchUrl(const GURL& launch_url);
  void SetScope(const GURL& scope);
  void SetThemeColor(base::Optional<SkColor> theme_color);
  void SetDisplayMode(blink::mojom::DisplayMode display_mode);
  void SetIsLocallyInstalled(bool is_locally_installed);
  void SetIsSyncPlaceholder(bool is_sync_placeholder);
  void SetIcons(Icons icons);

  void SetSyncData(const SyncData& sync_data);

 private:
  friend class WebAppDatabase;
  friend bool operator==(const WebApp&, const WebApp&);
  friend std::ostream& operator<<(std::ostream&, const WebApp&);

  const AppId app_id_;

  // This set always contains at least one source.
  using Sources = std::bitset<Source::kMaxValue>;
  Sources sources_;

  std::string name_;
  std::string description_;
  GURL launch_url_;
  // TODO(loyso): Implement IsValid() function that verifies that the launch_url
  // is within the scope.
  GURL scope_;
  base::Optional<SkColor> theme_color_;
  blink::mojom::DisplayMode display_mode_;
  bool is_locally_installed_ = true;
  bool is_sync_placeholder_ = false;
  Icons icons_;

  SyncData sync_data_;

  DISALLOW_COPY_AND_ASSIGN(WebApp);
};

// For logging and debug purposes.
std::ostream& operator<<(std::ostream& out, const WebApp& app);

bool operator==(const WebApp::IconInfo& icon_info1,
                const WebApp::IconInfo& icon_info2);

bool operator==(const WebApp::SyncData& sync_data1,
                const WebApp::SyncData& sync_data2);

bool operator==(const WebApp& app1, const WebApp& app2);

bool operator!=(const WebApp& app1, const WebApp& app2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
