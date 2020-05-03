// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PEOPLE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PEOPLE_SECTION_H_

#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;
class Profile;
class SupervisedUserService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace chromeos {

namespace settings {

// Provides UI strings and search tags for People settings. Search tags are only
// added for non-guest sessions.
//
// Kerberos, Fingerprint, and Parental Controls search tags are only shown if
// they are allowed by policy/flags. Different sets of Sync tags are shown
// depending on whether the feature is enabed or disabled.
class PeopleSection : public OsSettingsSection,
                      public syncer::SyncServiceObserver,
                      public KerberosCredentialsManager::Observer {
 public:
  PeopleSection(Profile* profile,
                Delegate* per_page_delegate,
                syncer::SyncService* sync_service,
                SupervisedUserService* supervised_user_service,
                KerberosCredentialsManager* kerberos_credentials_manager,
                signin::IdentityManager* identity_manager,
                PrefService* pref_service);
  ~PeopleSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;

  // KerberosCredentialsManager::Observer:
  void OnKerberosEnabledStateChanged() override;

  void AddKerberosAccountsPageStrings(
      content::WebUIDataSource* html_source) const;
  bool AreFingerprintSettingsAllowed();

  syncer::SyncService* sync_service_;
  SupervisedUserService* supervised_user_service_;
  KerberosCredentialsManager* kerberos_credentials_manager_;
  signin::IdentityManager* identity_manager_;
  PrefService* pref_service_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_PEOPLE_SECTION_H_
