// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_MANIFEST_UPDATE_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_MANIFEST_UPDATE_TASK_H_

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/manifest/manifest.h"

struct InstallableData;

namespace web_app {

class AppRegistrar;
class WebAppUiManager;
class InstallManager;
enum class InstallResultCode;

enum ManifestUpdateResult {
  kNoAppInScope,
  kThrottled,
  kWebContentsDestroyed,
  kAppUninstalled,
  kAppUpToDate,
  kAppDataInvalid,
  kAppUpdateFailed,
  kAppUpdated,
};

// Used by UpdateManager on a per web app basis for checking and performing
// manifest updates.
class ManifestUpdateTask final
    : public base::SupportsWeakPtr<ManifestUpdateTask>,
      public content::WebContentsObserver {
 public:
  using StoppedCallback = base::OnceCallback<void(const ManifestUpdateTask&,
                                                  ManifestUpdateResult result)>;

  ManifestUpdateTask(const GURL& url,
                     const AppId& app_id,
                     content::WebContents* web_contents,
                     StoppedCallback stopped_callback,
                     bool hang_for_testing,
                     const AppRegistrar& registrar,
                     WebAppUiManager* ui_manager,
                     InstallManager* install_manager);

  ~ManifestUpdateTask() override;

  const GURL& url() const { return url_; }
  const AppId& app_id() const { return app_id_; }

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;

 private:
  enum class Stage {
    kPendingPageLoad,
    kPendingInstallableData,
    kPendingWindowsClosed,
    kPendingInstallation,
  };

  void OnDidGetInstallableData(const InstallableData& data);
  bool IsUpdateNeededForManifest(const blink::Manifest& manifest) const;
  void OnAllAppWindowsClosed(blink::Manifest manifest);
  void OnInstallationComplete(const AppId& app_id, InstallResultCode code);
  void DestroySelf(ManifestUpdateResult result);

  const AppRegistrar& registrar_;
  WebAppUiManager& ui_manager_;
  InstallManager& install_manager_;

  Stage stage_;
  const GURL url_;
  const AppId app_id_;
  StoppedCallback stopped_callback_;
  bool hang_for_testing_ = false;
#if DCHECK_IS_ON()
  bool* destructor_called_ptr_ = nullptr;
#endif
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_MANIFEST_UPDATE_TASK_H_
