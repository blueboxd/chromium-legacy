// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browsing_data_remover_delegate.h"

#include "base/callback.h"
#include "components/prefs/pref_service.h"
#include "components/site_isolation//pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace weblayer {

BrowsingDataRemoverDelegate::BrowsingDataRemoverDelegate(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
BrowsingDataRemoverDelegate::GetOriginTypeMatcher() {
  return EmbedderOriginTypeMatcher();
}

bool BrowsingDataRemoverDelegate::MayRemoveDownloadHistory() {
  return true;
}

std::vector<std::string>
BrowsingDataRemoverDelegate::GetDomainsForDeferredCookieDeletion(
    uint64_t remove_mask) {
  return {};
}

void BrowsingDataRemoverDelegate::RemoveEmbedderData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    content::BrowsingDataFilterBuilder* filter_builder,
    uint64_t origin_type_mask,
    base::OnceClosure callback) {
  // Note: if history is ever added to WebLayer, also remove isolated origins
  // when history is cleared.
  if (remove_mask & DATA_TYPE_ISOLATED_ORIGINS) {
    user_prefs::UserPrefs::Get(browser_context_)
        ->ClearPref(site_isolation::prefs::kUserTriggeredIsolatedOrigins);
    // Note that this does not clear these sites from the in-memory map in
    // ChildProcessSecurityPolicy, since that is not supported at runtime. That
    // list of isolated sites is not directly exposed to users, though, and
    // will be cleared on next restart.
  }
  std::move(callback).Run();
}

}  // namespace weblayer
