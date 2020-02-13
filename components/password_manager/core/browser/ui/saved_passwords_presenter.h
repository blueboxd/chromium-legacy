// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_

#include <vector>

#include "base/observer_list.h"
#include "base/strings/string_piece_forward.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

// This interface provides a way for clients to obtain a list of all saved
// passwords and register themselves as observers for changes. In contrast to
// simply registering oneself as an observer of a password store directly, this
// class possibly responds to changes in multiple password stores, such as the
// local and account store used for passwords for butter.
// Furthermore, this class exposes a direct mean to edit a password, and
// notifies its observers about this event. An example use case for this is the
// bulk check settings page, where an edit operation in that page should result
// in the new password to be checked, whereas other password edit operations
// (such as visiting a change password form and then updating the password in
// Chrome) should not trigger a check.
class SavedPasswordsPresenter {
 public:
  // Observer interface. Clients can implement this to get notified about
  // changes to the list of saved passwords or if a given password was edited
  // Clients can register and de-register themselves, and are expected to do so
  // before the presenter gets out of scope.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer when a password is edited or the list of saved
    // passwords changed.
    //
    // OnEdited() will be invoked asynchronously if EditPassword() is invoked
    // with a password that was present in the store. |password.password_value|
    // will be equal to |new_password| in this case.
    virtual void OnEdited(const autofill::PasswordForm& password) {}
    // OnSavedPasswordsChanged() gets invoked asynchronously after a change to
    // the underlying password store happens. This might be due to a call to
    // EditPassword(), but can also happen if passwords are added or removed due
    // to other reasons. Clients are then expected to call GetSavedPassword() in
    // order to obtain the new list of credentials.
    virtual void OnSavedPasswordsChanged() {}
  };

  SavedPasswordsPresenter();
  virtual ~SavedPasswordsPresenter();

  // Edits |password|. This will ask the password store to change the underlying
  // password_value to |new_password|. This will also notify clients that an
  // edit event happened in case |password| was present in the store.
  virtual void EditPassword(const autofill::PasswordForm& password,
                            base::StringPiece16 new_password) = 0;

  // Returns a list of the currently saved credentials. Note that this is not a
  // read-only view, as it combines the result of multiple password stores that
  // change independently. This list is created on demand and callers should be
  // mindful to not create unnecessary copies.
  virtual std::vector<autofill::PasswordForm> GetSavedPasswords() = 0;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Notify observers about changes in the compromised credentials.
  void NotifyEdited(const autofill::PasswordForm& password);
  void NotifySavedPasswordsChanged();

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_SAVED_PASSWORDS_PRESENTER_H_
