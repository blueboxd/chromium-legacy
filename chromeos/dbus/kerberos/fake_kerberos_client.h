// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_
#define CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "chromeos/dbus/kerberos/kerberos_service.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeKerberosClient
    : public KerberosClient {
 public:
  FakeKerberosClient();
  ~FakeKerberosClient() override;

  // KerberosClient:
  void AddAccount(const kerberos::AddAccountRequest& request,
                  AddAccountCallback callback) override;
  void RemoveAccount(const kerberos::RemoveAccountRequest& request,
                     RemoveAccountCallback callback) override;
  void ClearAccounts(const kerberos::ClearAccountsRequest& request,
                     ClearAccountsCallback callback) override;
  void ListAccounts(const kerberos::ListAccountsRequest& request,
                    ListAccountsCallback callback) override;
  void SetConfig(const kerberos::SetConfigRequest& request,
                 SetConfigCallback callback) override;
  void AcquireKerberosTgt(const kerberos::AcquireKerberosTgtRequest& request,
                          int password_fd,
                          AcquireKerberosTgtCallback callback) override;
  void GetKerberosFiles(const kerberos::GetKerberosFilesRequest& request,
                        GetKerberosFilesCallback callback) override;
  void ConnectToKerberosFileChangedSignal(
      KerberosFilesChangedCallback callback) override;

 private:
  struct AccountData {
    // Kerberos configuration file.
    std::string krb5conf;

    // True if AcquireKerberosTgt succeeded.
    bool has_tgt = false;

    // True if the account was added by policy.
    bool is_managed = false;

    // Remembered password, if any.
    std::string password;
  };

  // Returns the AccountData for |principal_name| if available or nullptr
  // otherwise.
  AccountData* GetAccountData(const std::string& principal_name);

  // Maps principal name (user@REALM.COM) to account data.
  using AccountsMap = std::unordered_map<std::string, AccountData>;
  AccountsMap accounts_;

  KerberosFilesChangedCallback kerberos_files_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeKerberosClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_
