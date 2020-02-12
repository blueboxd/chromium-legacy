// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_PROVIDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_PROVIDER_H_

#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace password_manager {

// This class provides a read-only view over saved compromised credentials. It
// supports an observer interface, and clients can register themselves to get
// notified about changes to the list.
class CompromisedCredentialsProvider {
 public:
  // Simple credential. This is similar to the CompromisedCredentials struct,
  // but it does not store information about creation time or why the credential
  // is compromised, however it does know about the password of the associated
  // credential.
  struct Credential {
    base::string16 username;
    base::string16 password;
    GURL url;
  };

  using CredentialsView = base::span<const Credential>;

  // Observer interface. Clients can implement this to get notified about
  // changes to the list of compromised credentials. Clients can register and
  // de-register themselves, and are expected to do so before the provider gets
  // out of scope.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCompromisedCredentialsChanged(
        CredentialsView credentials) = 0;
  };

  CompromisedCredentialsProvider();
  ~CompromisedCredentialsProvider();

  // Returns a read-only view over the currently compromised credentials.
  CredentialsView GetCompromisedCredentials() const;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Notify observers about changes in the compromised credentials.
  void NotifyCompromisedCredentialsChanged(CredentialsView credentials);

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_COMPROMISED_CREDENTIALS_PROVIDER_H_
