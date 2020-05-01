// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_change_notifier.h"

#include <utility>

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"

namespace blink {

ManifestChangeNotifier::ManifestChangeNotifier(LocalDOMWindow& window)
    : window_(window) {}

ManifestChangeNotifier::~ManifestChangeNotifier() = default;

void ManifestChangeNotifier::Trace(Visitor* visitor) {
  visitor->Trace(window_);
}

void ManifestChangeNotifier::DidChangeManifest() {
  // Manifests are not considered when the current page has a unique origin.
  if (!ManifestManager::From(*window_)->CanFetchManifest())
    return;

  if (report_task_scheduled_)
    return;

  // Changing the manifest URL can trigger multiple notifications; the manifest
  // URL update may involve removing the old manifest link before adding the new
  // one, triggering multiple calls to DidChangeManifest(). Coalesce changes
  // during a single event loop task to avoid sending spurious notifications to
  // the browser.
  //
  // During document load, coalescing is disabled to maintain relative ordering
  // of this notification and the favicon URL reporting.
  if (!window_->GetFrame()->IsLoading()) {
    report_task_scheduled_ = true;
    window_->GetTaskRunner(TaskType::kInternalLoading)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&ManifestChangeNotifier::ReportManifestChange,
                             WrapWeakPersistent(this)));
    return;
  }
  ReportManifestChange();
}

void ManifestChangeNotifier::ReportManifestChange() {
  report_task_scheduled_ = false;
  if (!window_ || !window_->GetFrame())
    return;

  auto manifest_url = ManifestManager::From(*window_)->ManifestURL();

  EnsureManifestChangeObserver();

  // |manifest_change_observer_| may be null for tests.
  if (!manifest_change_observer_)
    return;

  if (manifest_url.IsNull())
    manifest_change_observer_->ManifestUrlChanged(base::nullopt);
  else
    manifest_change_observer_->ManifestUrlChanged(manifest_url);
}

void ManifestChangeNotifier::EnsureManifestChangeObserver() {
  if (manifest_change_observer_)
    return;

  AssociatedInterfaceProvider* provider =
      window_->GetFrame()->GetRemoteNavigationAssociatedInterfaces();
  if (!provider)
    return;

  provider->GetInterface(&manifest_change_observer_);
}

}  // namespace blink
