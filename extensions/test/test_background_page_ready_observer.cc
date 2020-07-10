// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_background_page_ready_observer.h"

#include "base/bind.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

bool IsExtensionBackgroundPageReady(
    content::BrowserContext* browser_context,
    const extensions::ExtensionId& extension_id) {
  const auto* const extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);
  if (!extension_registry)
    return false;
  const extensions::Extension* const extension =
      extension_registry->GetInstalledExtension(extension_id);
  if (!extension)
    return false;
  auto* const extension_system =
      extensions::ExtensionSystem::Get(browser_context);
  if (!extension_system)
    return false;
  return extension_system->runtime_data()->IsBackgroundPageReady(extension);
}

}  // namespace

ExtensionBackgroundPageReadyObserver::ExtensionBackgroundPageReadyObserver(
    content::BrowserContext* browser_context,
    const extensions::ExtensionId& extension_id)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      notification_observer_(
          extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
          base::BindRepeating(
              &ExtensionBackgroundPageReadyObserver::IsNotificationRelevant,
              base::Unretained(this))) {}

ExtensionBackgroundPageReadyObserver::~ExtensionBackgroundPageReadyObserver() =
    default;

void ExtensionBackgroundPageReadyObserver::Wait() {
  notification_observer_.Wait();
}

bool ExtensionBackgroundPageReadyObserver::IsNotificationRelevant(
    const content::NotificationSource& source,
    const content::NotificationDetails& /*details*/) const {
  if (content::Source<const extensions::Extension>(source)->id() !=
      extension_id_) {
    return false;
  }
  // Double-check via the extension system, since the notification could be
  // for a different profile.
  return IsExtensionBackgroundPageReady(browser_context_, extension_id_);
}

}  // namespace extensions
