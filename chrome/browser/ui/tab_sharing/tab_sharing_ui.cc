// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "net/base/url_util.h"

namespace {

base::string16 GetTabName(content::WebContents* tab) {
  GURL url = tab->GetURL();
  return content::IsOriginSecure(url)
             ? base::UTF8ToUTF16(net::GetHostAndOptionalPort(url))
             : url_formatter::FormatUrlForSecurityDisplay(url.GetOrigin());
}

}  // namespace

TabSharingUI::TabSharingUI(const content::DesktopMediaID& media_id,
                           base::string16 app_name)
    : app_name_(std::move(app_name)) {
  shared_tab_ = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(
          media_id.web_contents_id.render_process_id,
          media_id.web_contents_id.main_render_frame_id));
  shared_tab_name_ = GetTabName(shared_tab_);
  profile_ = ProfileManager::GetLastUsedProfileAllowedByPolicy();
}

TabSharingUI::~TabSharingUI() {
  if (!infobars_.empty())
    RemoveInfobarForAllTabs();
}

gfx::NativeViewId TabSharingUI::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback) {
  source_callback_ = std::move(source_callback);
  stop_callback_ = std::move(stop_callback);
  CreateInfobarForAllTabs();
  return 0;
}

void TabSharingUI::OnBrowserAdded(Browser* browser) {
  if (browser->profile()->GetOriginalProfile() == profile_)
    tab_strip_models_observer_.Add(browser->tab_strip_model());
}

void TabSharingUI::OnBrowserRemoved(Browser* browser) {
  BrowserList* browser_list = BrowserList::GetInstance();
  if (browser_list->empty())
    browser_list->RemoveObserver(this);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  if (tab_strip_models_observer_.IsObserving(tab_strip_model))
    tab_strip_models_observer_.Remove(tab_strip_model);
}

void TabSharingUI::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (infobars_.find(contents.contents) != infobars_.end())
        continue;
      infobars_[contents.contents] = TabSharingInfoBarDelegate::Create(
          InfoBarService::FromWebContents(contents.contents),
          shared_tab_ == contents.contents ? base::string16()
                                           : shared_tab_name_,
          app_name_, !source_callback_.is_null() /*is_sharing_allowed*/, this);
    }
  } else if (change.type() == TabStripModelChange::kRemoved &&
             change.GetRemove()->will_be_deleted) {
    bool stop_sharing = false;
    for (const auto& contents : change.GetRemove()->contents) {
      if (contents.contents == shared_tab_)
        stop_sharing = true;
      infobars_.erase(contents.contents);
    }

    if (stop_sharing)
      StopSharing();
  }
}

void TabSharingUI::StartSharing(infobars::InfoBar* infobar) {
  if (source_callback_.is_null())
    return;

  content::WebContents* shared_tab =
      InfoBarService::WebContentsFromInfoBar(infobar);
  DCHECK(shared_tab);
  DCHECK_EQ(infobars_[shared_tab], infobar);
  shared_tab_ = shared_tab;
  shared_tab_name_ = GetTabName(shared_tab_);

  content::RenderFrameHost* main_frame = shared_tab->GetMainFrame();
  DCHECK(main_frame);
  RemoveInfobarForAllTabs();
  source_callback_.Run(content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                         main_frame->GetRoutingID())));
}

void TabSharingUI::StopSharing() {
  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();
  shared_tab_ = nullptr;
  RemoveInfobarForAllTabs();
}

void TabSharingUI::CreateInfobarForAllTabs() {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (auto* browser : *browser_list) {
    OnBrowserAdded(browser);

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      infobars_[contents] = TabSharingInfoBarDelegate::Create(
          InfoBarService::FromWebContents(contents),
          shared_tab_ == contents ? base::string16() : shared_tab_name_,
          app_name_, !source_callback_.is_null() /*is_sharing_allowed*/, this);
    }
  }
  browser_list->AddObserver(this);
}

void TabSharingUI::RemoveInfobarForAllTabs() {
  BrowserList::GetInstance()->RemoveObserver(this);
  tab_strip_models_observer_.RemoveAll();

  for (const auto& infobars_entry : infobars_)
    infobars_entry.second->RemoveSelf();

  infobars_.clear();
}
