// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVER_WIN_SERVER_H_
#define CHROME_UPDATER_SERVER_WIN_SERVER_H_

#include <wrl/implements.h>
#include <wrl/module.h>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "chrome/updater/server/win/updater_idl.h"

namespace updater {

// This class implements the IUpdater interface and exposes it as a COM object.
class UpdaterImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdater> {
 public:
  UpdaterImpl() = default;
  UpdaterImpl(const UpdaterImpl&) = delete;
  UpdaterImpl& operator=(const UpdaterImpl&) = delete;

  // Overrides for IUpdater.
  IFACEMETHODIMP CheckForUpdate(const base::char16* app_id) override;
  IFACEMETHODIMP Register(const base::char16* app_id,
                          const base::char16* brand_code,
                          const base::char16* tag,
                          const base::char16* version,
                          const base::char16* existence_checker_path) override;
  IFACEMETHODIMP Update(const base::char16* app_id) override;
  IFACEMETHODIMP UpdateAll(IUpdaterObserver* observer) override;

 private:
  ~UpdaterImpl() override = default;
};

class App;

scoped_refptr<App> MakeAppServer();

}  // namespace updater

#endif  // CHROME_UPDATER_SERVER_WIN_SERVER_H_
