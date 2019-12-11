// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/did_run_updater_win.h"

#include "chrome/installer/util/update_did_run_state.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

DidRunUpdater::DidRunUpdater() {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
}

DidRunUpdater::~DidRunUpdater() {
}

void DidRunUpdater::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  installer::UpdateDidRunState(true);
}
