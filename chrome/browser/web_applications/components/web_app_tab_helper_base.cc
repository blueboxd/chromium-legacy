// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"

#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/components/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"

namespace web_app {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelperBase)

WebAppTabHelperBase::WebAppTabHelperBase(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  DCHECK(provider);
  observer_.Add(&provider->registrar());
}

WebAppTabHelperBase::~WebAppTabHelperBase() = default;

void WebAppTabHelperBase::Init(WebAppAudioFocusIdMap* audio_focus_id_map) {
  SetAudioFocusIdMap(audio_focus_id_map);

  // Sync app_id with the initial url from WebContents (used in Tab Restore etc)
  const GURL init_url = web_contents()->GetSiteInstance()->GetSiteURL();
  SetAppId(FindAppIdWithUrlInScope(init_url));
}

bool WebAppTabHelperBase::HasAssociatedApp() const {
  return !app_id_.empty();
}

void WebAppTabHelperBase::SetAppId(const AppId& app_id) {
  if (app_id_ == app_id)
    return;

  app_id_ = app_id;

  OnAssociatedAppChanged();
}

void WebAppTabHelperBase::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  const AppId app_id = FindAppIdWithUrlInScope(navigation_handle->GetURL());
  SetAppId(app_id);

  ReinstallPlaceholderAppIfNecessary(navigation_handle->GetURL());
}

void WebAppTabHelperBase::DidCloneToNewWebContents(
    content::WebContents* old_web_contents,
    content::WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a WebAppTabHelperBase.
  WebAppTabHelperBase* new_tab_helper = CloneForWebContents(new_web_contents);

  // Clone common state:
  new_tab_helper->SetAudioFocusIdMap(audio_focus_id_map_);
  new_tab_helper->SetAppId(app_id());
}

void WebAppTabHelperBase::SetAudioFocusIdMap(
    WebAppAudioFocusIdMap* audio_focus_id_map) {
  DCHECK(!audio_focus_id_map_ && audio_focus_id_map);
  audio_focus_id_map_ = audio_focus_id_map;
}

void WebAppTabHelperBase::OnWebAppInstalled(const AppId& installed_app_id) {
  // Check if current web_contents url is in scope for the newly installed app.
  AppId app_id = FindAppIdWithUrlInScope(web_contents()->GetURL());
  if (app_id == installed_app_id)
    SetAppId(app_id);
}

void WebAppTabHelperBase::OnWebAppUninstalled(const AppId& uninstalled_app_id) {
  if (app_id() == uninstalled_app_id)
    ResetAppId();
}

void WebAppTabHelperBase::OnAppRegistrarShutdown() {
  ResetAppId();
}

void WebAppTabHelperBase::OnAppRegistrarDestroyed() {
  observer_.RemoveAll();
}

void WebAppTabHelperBase::ResetAppId() {
  app_id_.clear();

  OnAssociatedAppChanged();
}

void WebAppTabHelperBase::OnAssociatedAppChanged() {
  UpdateAudioFocusGroupId();
}

void WebAppTabHelperBase::UpdateAudioFocusGroupId() {
  DCHECK(audio_focus_id_map_);

  if (!app_id_.empty() && IsInAppWindow()) {
    audio_focus_group_id_ = audio_focus_id_map_->CreateOrGetIdForApp(app_id_);
  } else {
    audio_focus_group_id_ = base::UnguessableToken::Null();
  }

  content::MediaSession::Get(web_contents())
      ->SetAudioFocusGroupId(audio_focus_group_id_);
}

void WebAppTabHelperBase::ReinstallPlaceholderAppIfNecessary(const GURL& url) {
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  DCHECK(provider);

  // WebAppPolicyManager might be nullptr in the non-extensions implementation.
  if (!provider->policy_manager())
    return;

  provider->policy_manager()->ReinstallPlaceholderAppIfNecessary(url);
}

AppId WebAppTabHelperBase::FindAppIdWithUrlInScope(const GURL& url) const {
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  DCHECK(provider);
  return provider->registrar().FindAppWithUrlInScope(url).value_or(AppId());
}

}  // namespace web_app
