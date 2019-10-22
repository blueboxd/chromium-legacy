// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_base.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace chromeos {

class KioskAppDataDelegate;

class WebKioskAppData : public KioskAppDataBase {
 public:
  enum Status {
    STATUS_INIT,     // Data initialized with app id.
    STATUS_LOADING,  // Loading data from cache or web store.
    STATUS_LOADED,   // Data load finished.
  };
  WebKioskAppData(KioskAppDataDelegate* delegate,
                  const std::string& app_id,
                  const AccountId& account_id,
                  const GURL url);

  ~WebKioskAppData() override;

  // Loads the locally cached data. Returns true on success.
  bool LoadFromCache();

  // KioskAppDataBase overrides:
  void OnIconLoadSuccess(const gfx::ImageSkia& icon) override;
  void OnIconLoadFailure() override;

  void SetStatus(Status status);

  Status status() const { return status_; }
  const GURL& url() const { return url_; }

 private:
  const KioskAppDataDelegate* delegate_;  // not owned.
  Status status_;
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(WebKioskAppData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_
