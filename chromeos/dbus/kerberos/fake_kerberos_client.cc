// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/kerberos/fake_kerberos_client.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "third_party/cros_system_api/dbus/kerberos/dbus-constants.h"

namespace chromeos {
namespace {

// Fake delay for any asynchronous operation.
constexpr auto kTaskDelay = base::TimeDelta::FromMilliseconds(100);

// Fake validity lifetime for TGTs.
constexpr base::TimeDelta kTgtValidity = base::TimeDelta::FromHours(10);

// Fake renewal lifetime for TGTs.
constexpr base::TimeDelta kTgtRenewal = base::TimeDelta::FromHours(24);

// Posts |callback| on the current thread's task runner, passing it the
// |response| message.
template <class TProto>
void PostProtoResponse(base::OnceCallback<void(const TProto&)> callback,
                       const TProto& response) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), response), kTaskDelay);
}

// Similar to PostProtoResponse(), but posts |callback| with a proto containing
// only the given |error|.
template <class TProto>
void PostResponse(base::OnceCallback<void(const TProto&)> callback,
                  kerberos::ErrorType error) {
  TProto response;
  response.set_error(error);
  PostProtoResponse(std::move(callback), response);
}

// Reads the password from the file descriptor |password_fd|.
// Not very efficient, but simple!
std::string ReadPassword(int password_fd) {
  std::string password;
  char c;
  while (base::ReadFromFD(password_fd, &c, 1))
    password.push_back(c);
  return password;
}

}  // namespace

FakeKerberosClient::FakeKerberosClient() = default;

FakeKerberosClient::~FakeKerberosClient() = default;

void FakeKerberosClient::AddAccount(const kerberos::AddAccountRequest& request,
                                    AddAccountCallback callback) {
  auto it = std::find(accounts_.begin(), accounts_.end(),
                      AccountData(request.principal_name()));
  if (it != accounts_.end()) {
    it->is_managed |= request.is_managed();
    PostResponse(std::move(callback), kerberos::ERROR_DUPLICATE_PRINCIPAL_NAME);
    return;
  }

  AccountData data(request.principal_name());
  data.is_managed = request.is_managed();
  accounts_.push_back(data);
  PostResponse(std::move(callback), kerberos::ERROR_NONE);
}

void FakeKerberosClient::RemoveAccount(
    const kerberos::RemoveAccountRequest& request,
    RemoveAccountCallback callback) {
  auto it = std::find(accounts_.begin(), accounts_.end(),
                      AccountData(request.principal_name()));
  if (it == accounts_.end()) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME);
    return;
  }
  accounts_.erase(it);
  PostResponse(std::move(callback), kerberos::ERROR_NONE);
}

void FakeKerberosClient::ClearAccounts(
    const kerberos::ClearAccountsRequest& request,
    ClearAccountsCallback callback) {
  switch (request.mode()) {
    case kerberos::CLEAR_ALL:
      accounts_.clear();
      break;

    case kerberos::CLEAR_ONLY_UNMANAGED_ACCOUNTS:
      accounts_.erase(std::remove_if(accounts_.begin(), accounts_.end(),
                                     [](const AccountData& account) {
                                       return !account.is_managed;
                                     }),
                      accounts_.end());
      break;

    case kerberos::CLEAR_ONLY_UNMANAGED_REMEMBERED_PASSWORDS:
      for (auto& account : accounts_) {
        if (!account.is_managed)
          account.password.clear();
      }
      break;
  }
  PostResponse(std::move(callback), kerberos::ERROR_NONE);
}

void FakeKerberosClient::ListAccounts(
    const kerberos::ListAccountsRequest& request,
    ListAccountsCallback callback) {
  kerberos::ListAccountsResponse response;
  for (const AccountData& data : accounts_) {
    kerberos::Account* account = response.add_accounts();
    account->set_principal_name(data.principal_name);
    account->set_krb5conf(data.krb5conf);
    account->set_tgt_validity_seconds(data.has_tgt ? kTgtValidity.InSeconds()
                                                   : 0);
    account->set_tgt_renewal_seconds(data.has_tgt ? kTgtRenewal.InSeconds()
                                                  : 0);
    account->set_is_managed(data.is_managed);
    account->set_password_was_remembered(!data.password.empty());
    account->set_use_login_password(data.use_login_password);
  }
  response.set_error(kerberos::ERROR_NONE);
  PostProtoResponse(std::move(callback), response);
}

void FakeKerberosClient::SetConfig(const kerberos::SetConfigRequest& request,
                                   SetConfigCallback callback) {
  AccountData* data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME);
    return;
  }

  data->krb5conf = request.krb5conf();
  PostResponse(std::move(callback), kerberos::ERROR_NONE);
}

void FakeKerberosClient::AcquireKerberosTgt(
    const kerberos::AcquireKerberosTgtRequest& request,
    int password_fd,
    AcquireKerberosTgtCallback callback) {
  AccountData* data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME);
    return;
  }

  // Remember whether to use the login password.
  data->use_login_password = request.use_login_password();

  std::string password;
  if (request.use_login_password()) {
    // "Retrieve" login password.
    password = "fake_login_password";

    // Erase a previously remembered password.
    data->password.clear();
  } else {
    // Remember password.
    password = ReadPassword(password_fd);
    if (!password.empty() && request.remember_password())
      data->password = password;

    // Use remembered password.
    if (password.empty())
      password = data->password;

    // Erase a previously remembered password.
    if (!request.remember_password())
      data->password.clear();
  }

  // Reject empty passwords.
  if (password.empty()) {
    PostResponse(std::move(callback), kerberos::ERROR_BAD_PASSWORD);
    return;
  }

  // It worked! Magic!
  data->has_tgt = true;
  PostResponse(std::move(callback), kerberos::ERROR_NONE);
}

void FakeKerberosClient::GetKerberosFiles(
    const kerberos::GetKerberosFilesRequest& request,
    GetKerberosFilesCallback callback) {
  AccountData* data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME);
    return;
  }

  kerberos::GetKerberosFilesResponse response;
  if (data->has_tgt) {
    response.mutable_files()->set_krb5cc("Fake Kerberos credential cache");
    response.mutable_files()->set_krb5conf("Fake Kerberos configuration");
  }
  response.set_error(kerberos::ERROR_NONE);
  PostProtoResponse(std::move(callback), response);
}

void FakeKerberosClient::ConnectToKerberosFileChangedSignal(
    KerberosFilesChangedCallback callback) {
  DCHECK(!kerberos_files_changed_callback_);
  kerberos_files_changed_callback_ = callback;
}

void FakeKerberosClient::ConnectToKerberosTicketExpiringSignal(
    KerberosTicketExpiringCallback callback) {
  DCHECK(!kerberos_ticket_expiring_callback_);
  kerberos_ticket_expiring_callback_ = callback;
}

FakeKerberosClient::AccountData* FakeKerberosClient::GetAccountData(
    const std::string& principal_name) {
  auto it = std::find(accounts_.begin(), accounts_.end(),
                      AccountData(principal_name));
  return it != accounts_.end() ? &*it : nullptr;
}

FakeKerberosClient::AccountData::AccountData(const std::string& principal_name)
    : principal_name(principal_name) {}

FakeKerberosClient::AccountData::AccountData(const AccountData& other) =
    default;

bool FakeKerberosClient::AccountData::operator==(
    const AccountData& other) const {
  return principal_name == other.principal_name;
}

bool FakeKerberosClient::AccountData::operator!=(
    const AccountData& other) const {
  return !(*this == other);
}

}  // namespace chromeos
