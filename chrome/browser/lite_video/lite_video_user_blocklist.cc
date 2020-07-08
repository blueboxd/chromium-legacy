// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_user_blocklist.h"

#include "base/optional.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// Determine whether the provided navigation is valid and can be queried or
// added to the blocklist.
bool IsNavigationValidForBlocklist(
    content::NavigationHandle* navigation_handle) {
  GURL navigation_url = navigation_handle->GetURL();
  return navigation_url.SchemeIsHTTPOrHTTPS() && navigation_url.has_host();
}

}  // namespace

namespace lite_video {

// Separator between hosts for the rebuffer blocklist type.
constexpr char kLiteVideoBlocklistKeySeparator[] = "_";

// static
base::Optional<std::string> LiteVideoUserBlocklist::GetRebufferBlocklistKey(
    content::NavigationHandle* navigation_handle) {
  if (!IsNavigationValidForBlocklist(navigation_handle))
    return base::nullopt;

  const GURL url = navigation_handle->GetURL();
  if (navigation_handle->IsInMainFrame())
    return url.host() + kLiteVideoBlocklistKeySeparator;

  const GURL mainframe_url =
      navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (!mainframe_url.SchemeIsHTTPOrHTTPS() || !mainframe_url.has_host())
    return base::nullopt;
  return mainframe_url.host() + kLiteVideoBlocklistKeySeparator + url.host();
}

LiteVideoUserBlocklist::LiteVideoUserBlocklist(
    std::unique_ptr<blocklist::OptOutStore> opt_out_store,
    base::Clock* clock,
    blocklist::OptOutBlocklistDelegate* blocklist_delegate)
    : OptOutBlocklist(std::move(opt_out_store), clock, blocklist_delegate) {
  Init();
}

LiteVideoUserBlocklist::~LiteVideoUserBlocklist() = default;

LiteVideoBlocklistReason LiteVideoUserBlocklist::IsLiteVideoAllowedOnNavigation(
    content::NavigationHandle* navigation_handle) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsNavigationValidForBlocklist(navigation_handle))
    return LiteVideoBlocklistReason::kNavigationNotEligibile;

  std::vector<blocklist::BlocklistReason> passed_reasons;
  auto blocklist_reason = blocklist::OptOutBlocklist::IsLoadedAndAllowed(
      navigation_handle->GetURL().host(),
      static_cast<int>(LiteVideoBlocklistType::kNavigationBlocklist),
      /*opt_out=*/false, &passed_reasons);
  if (blocklist_reason != blocklist::BlocklistReason::kAllowed)
    return LiteVideoBlocklistReason::kNavigationBlocklisted;

  base::Optional<std::string> rebuffer_key =
      GetRebufferBlocklistKey(navigation_handle);
  if (!rebuffer_key)
    return LiteVideoBlocklistReason::kNavigationNotEligibile;

  blocklist_reason = blocklist::OptOutBlocklist::IsLoadedAndAllowed(
      *rebuffer_key,
      static_cast<int>(LiteVideoBlocklistType::kRebufferBlocklist),
      /*opt_out=*/false, &passed_reasons);
  if (blocklist_reason != blocklist::BlocklistReason::kAllowed)
    return LiteVideoBlocklistReason::kRebufferingBlocklisted;
  return LiteVideoBlocklistReason::kAllowed;
}

bool LiteVideoUserBlocklist::ShouldUseSessionPolicy(base::TimeDelta* duration,
                                                    size_t* history,
                                                    int* threshold) const {
  return false;
}

bool LiteVideoUserBlocklist::ShouldUsePersistentPolicy(
    base::TimeDelta* duration,
    size_t* history,
    int* threshold) const {
  return false;
}

bool LiteVideoUserBlocklist::ShouldUseHostPolicy(base::TimeDelta* duration,
                                                 size_t* history,
                                                 int* threshold,
                                                 size_t* max_hosts) const {
  DCHECK(duration);
  DCHECK(history);
  DCHECK(threshold);
  DCHECK(max_hosts);
  *max_hosts = features::MaxUserBlocklistHosts();
  *duration = features::UserBlocklistHostDuration();
  *threshold = features::UserBlocklistOptOutHistoryThreshold();
  *history = features::UserBlocklistOptOutHistoryThreshold();
  return true;
}

bool LiteVideoUserBlocklist::ShouldUseTypePolicy(base::TimeDelta* duration,
                                                 size_t* history,
                                                 int* threshold) const {
  return false;
}

blocklist::BlocklistData::AllowedTypesAndVersions
LiteVideoUserBlocklist::GetAllowedTypes() const {
  return {{static_cast<int>(LiteVideoBlocklistType::kNavigationBlocklist),
           features::LiteVideoBlocklistVersion()},
          {static_cast<int>(LiteVideoBlocklistType::kRebufferBlocklist),
           features::LiteVideoBlocklistVersion()}};
}

void LiteVideoUserBlocklist::AddNavigationToBlocklist(
    content::NavigationHandle* navigation_handle,
    bool opt_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsNavigationValidForBlocklist(navigation_handle))
    return;
  AddEntry(navigation_handle->GetURL().host(), opt_out,
           static_cast<int>(LiteVideoBlocklistType::kNavigationBlocklist));
}

}  // namespace lite_video
