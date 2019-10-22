// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_data.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

WebKioskAppData::WebKioskAppData(KioskAppDataDelegate* delegate,
                                 const std::string& app_id,
                                 const AccountId& account_id,
                                 const GURL url)
    : KioskAppDataBase(WebKioskAppManager::kWebKioskDictionaryName,
                       app_id,
                       account_id),
      delegate_(delegate),
      status_(STATUS_INIT),
      url_(url) {
  name_ = url_.spec();
}

WebKioskAppData::~WebKioskAppData() = default;

bool WebKioskAppData::LoadFromCache() {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value* dict = local_state->GetDictionary(dictionary_name());

  if (!LoadFromDictionary(base::Value::AsDictionaryValue(*dict)))
    return false;

  // Wait while icon is loaded.
  if (status_ == STATUS_INIT)
    SetStatus(STATUS_LOADING);
  return true;
}

void WebKioskAppData::SetStatus(Status status) {
  if (status_ == status)
    return;
  status_ = status;

  if (delegate_)
    delegate_->OnKioskAppDataChanged(app_id());
}

void WebKioskAppData::OnIconLoadSuccess(const gfx::ImageSkia& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  kiosk_app_icon_loader_.reset();
  icon_ = icon;
  SetStatus(STATUS_LOADED);
}

void WebKioskAppData::OnIconLoadFailure() {
  kiosk_app_icon_loader_.reset();
  LOG(ERROR) << "Icon Load Failure";
  SetStatus(STATUS_LOADED);
  // Do nothing
}

}  // namespace chromeos
