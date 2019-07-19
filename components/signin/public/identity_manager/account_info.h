// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_

#include <string>

#include "google_apis/gaia/core_account_id.h"
#include "ui/gfx/image/image.h"

// Value representing no hosted domain associated with an account.
extern const char kNoHostedDomainFound[];

// Value representing no picture URL associated with an account.
extern const char kNoPictureURLFound[];

// Stores the basic information about an account that is always known
// about the account (from the moment it is added to the system until
// it is removed). It will unfrequently, if ever, change.
struct CoreAccountInfo {
  CoreAccountInfo();
  ~CoreAccountInfo();

  CoreAccountInfo(const CoreAccountInfo& other);
  CoreAccountInfo(CoreAccountInfo&& other) noexcept;

  CoreAccountInfo& operator=(const CoreAccountInfo& other);
  CoreAccountInfo& operator=(CoreAccountInfo&& other) noexcept;

  CoreAccountId account_id;
  std::string gaia;
  std::string email;

  bool is_under_advanced_protection = false;

  // Returns true if all fields in the account info are empty.
  bool IsEmpty() const;
};

// Stores all the information known about an account. Part of the information
// may only become available asynchronously.
struct AccountInfo : public CoreAccountInfo {
  AccountInfo();
  ~AccountInfo();

  AccountInfo(const AccountInfo& other);
  AccountInfo(AccountInfo&& other) noexcept;

  AccountInfo& operator=(const AccountInfo& other);
  AccountInfo& operator=(AccountInfo&& other) noexcept;

  std::string full_name;
  std::string given_name;
  std::string hosted_domain;
  std::string locale;
  std::string picture_url;
  gfx::Image account_image;
  bool is_child_account = false;

  // Returns true if all fields in the account info are empty.
  bool IsEmpty() const;

  // Returns true if all fields in this account info are filled.
  bool IsValid() const;

  // Updates the empty fields of |this| with |other|. Returns whether at least
  // one field was updated.
  bool UpdateWith(const AccountInfo& other);
};

bool operator==(const CoreAccountInfo& l, const CoreAccountInfo& r);
bool operator!=(const CoreAccountInfo& l, const CoreAccountInfo& r);
std::ostream& operator<<(std::ostream& os, const CoreAccountInfo& account);

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_INFO_H_
