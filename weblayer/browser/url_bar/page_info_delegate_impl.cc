// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/url_bar/page_info_delegate_impl.h"

#include "components/security_state/content/content_utils.h"

PageInfoDelegateImpl::PageInfoDelegateImpl(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

permissions::ChooserContextBase* PageInfoDelegateImpl::GetChooserContext(
    ContentSettingsType type) {
  // TODO(crbug.com/1052375): Implement.
  NOTREACHED();
  return nullptr;
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
safe_browsing::PasswordProtectionService*
PageInfoDelegateImpl::GetPasswordProtectionService() const {
  NOTREACHED();
  return nullptr;
}

void PageInfoDelegateImpl::OnUserActionOnPasswordUi(
    content::WebContents* web_contents,
    safe_browsing::WarningAction action) {
  NOTREACHED();
}

base::string16 PageInfoDelegateImpl::GetWarningDetailText() {
  // TODO(crbug.com/1052375): Implement.
  NOTREACHED();
  return base::string16();
}
#endif

permissions::PermissionResult PageInfoDelegateImpl::GetPermissionStatus(
    ContentSettingsType type,
    const GURL& site_url) {
  // TODO(crbug.com/1052375): Implement.
  NOTREACHED();
  return permissions::PermissionResult(
      CONTENT_SETTING_BLOCK, permissions::PermissionStatusSource::UNSPECIFIED);
}

#if !defined(OS_ANDROID)
bool PageInfoDelegateImpl::CreateInfoBarDelegate() {
  NOTREACHED();
  return false;
}

void PageInfoDelegateImpl::ShowSiteSettings(const GURL& site_url) {
  // TODO(crbug.com/1052375): Implement once site settings code has been
  // componentized.
  NOTREACHED();
}
#endif

permissions::PermissionDecisionAutoBlocker*
PageInfoDelegateImpl::GetPermissionDecisionAutoblocker() {
  // TODO(crbug.com/1052375): Implement.
  NOTREACHED();
  return nullptr;
}

StatefulSSLHostStateDelegate*
PageInfoDelegateImpl::GetStatefulSSLHostStateDelegate() {
  // TODO(crbug.com/1052375): Implement.
  NOTREACHED();
  return nullptr;
}

HostContentSettingsMap* PageInfoDelegateImpl::GetContentSettings() {
  // TODO(crbug.com/1052375): Implement once site settings code has been
  // componentized.
  NOTREACHED();
  return nullptr;
}

bool PageInfoDelegateImpl::IsContentDisplayedInVrHeadset() {
  // VR is not supported for WebLayer.
  return false;
}

security_state::SecurityLevel PageInfoDelegateImpl::GetSecurityLevel() {
  auto state = security_state::GetVisibleSecurityState(web_contents_);
  DCHECK(state);
  return security_state::GetSecurityLevel(
      *state,
      /* used_policy_installed_certificate */ false);
}

security_state::VisibleSecurityState
PageInfoDelegateImpl::GetVisibleSecurityState() {
  return *security_state::GetVisibleSecurityState(web_contents_);
}

std::unique_ptr<content_settings::TabSpecificContentSettings::Delegate>
PageInfoDelegateImpl::GetTabSpecificContentSettingsDelegate() {
  // TODO(crbug.com/1052375): Implement.
  NOTREACHED();
  return nullptr;
}