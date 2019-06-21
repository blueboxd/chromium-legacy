// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/output_protection_controller_ash.h"
#include "ui/display/manager/display_configurator.h"

#include "ash/shell.h"  // mash-ok

namespace {

display::ContentProtectionManager* manager() {
  return ash::Shell::Get()
      ->display_configurator()
      ->content_protection_manager();
}

}  // namespace

namespace chromeos {

OutputProtectionControllerAsh::OutputProtectionControllerAsh()
    : client_id_(manager()->RegisterClient()) {}

OutputProtectionControllerAsh::~OutputProtectionControllerAsh() {
  DCHECK(thread_checker_.CalledOnValidThread());
  manager()->UnregisterClient(client_id_);
}

void OutputProtectionControllerAsh::QueryStatus(
    int64_t display_id,
    const OutputProtectionDelegate::QueryStatusCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  manager()->QueryContentProtection(client_id_, display_id, callback);
}

void OutputProtectionControllerAsh::SetProtection(
    int64_t display_id,
    uint32_t desired_method_mask,
    const OutputProtectionDelegate::SetProtectionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  manager()->ApplyContentProtection(client_id_, display_id, desired_method_mask,
                                    callback);
}

}  // namespace chromeos
