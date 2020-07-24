// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"

// static
NearbyShareContactManagerImpl::Factory*
    NearbyShareContactManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareContactManager>
NearbyShareContactManagerImpl::Factory::Create() {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }

  return base::WrapUnique(new NearbyShareContactManagerImpl());
}

// static
void NearbyShareContactManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareContactManagerImpl::Factory::~Factory() = default;

NearbyShareContactManagerImpl::NearbyShareContactManagerImpl() = default;

NearbyShareContactManagerImpl::~NearbyShareContactManagerImpl() = default;

void NearbyShareContactManagerImpl::DownloadContacts(
    bool only_download_if_changed) {
  NOTIMPLEMENTED();
}

void NearbyShareContactManagerImpl::SetAllowedContacts(
    const std::set<std::string>& allowed_contact_ids) {
  NOTIMPLEMENTED();
}

void NearbyShareContactManagerImpl::OnStart() {
  NOTIMPLEMENTED();
}

void NearbyShareContactManagerImpl::OnStop() {
  NOTIMPLEMENTED();
}
