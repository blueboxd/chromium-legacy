// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_ui_views.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "net/base/url_util.h"
#include "ui/gfx/color_palette.h"

#if defined(OS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {

#if !defined(OS_MACOSX)
const int kContentsBorderThickness = 10;
const float kContentsBorderOpacity = 0.24;
const SkColor kContentsBorderColor = gfx::kGoogleBlue500;

void InitContentsBorderWidget(content::WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (browser_view->contents_border_widget())
    return;

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  views::Widget* frame = browser_view->contents_web_view()->GetWidget();
  params.parent = frame->GetNativeView();
  params.context = frame->GetNativeWindow();
  // Make the widget non-top level.
  params.child = true;
  params.name = "TabSharingContentsBorder";
  params.remove_standard_frame = true;
  // Let events go through to underlying view.
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
#if defined(OS_WIN)
  params.native_widget = new views::NativeWidgetAura(widget);
#endif

  widget->Init(std::move(params));
  views::View* border_view = new views::View();
  border_view->SetBorder(
      views::CreateSolidBorder(kContentsBorderThickness, kContentsBorderColor));
  widget->SetContentsView(border_view);
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->SetOpacity(kContentsBorderOpacity);

  browser_view->set_contents_border_widget(widget);
}
#endif

void SetContentsBorderVisible(content::WebContents* contents, bool visible) {
  if (!contents)
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser)
    return;
  views::Widget* contents_border_widget =
      BrowserView::GetBrowserViewForBrowser(browser)->contents_border_widget();
  if (!contents_border_widget)
    return;
  if (visible)
    contents_border_widget->Show();
  else
    contents_border_widget->Hide();
}

base::string16 GetTabName(content::WebContents* tab) {
  GURL url = tab->GetURL();
  return content::IsOriginSecure(url)
             ? base::UTF8ToUTF16(net::GetHostAndOptionalPort(url))
             : url_formatter::FormatUrlForSecurityDisplay(url.GetOrigin());
}

}  // namespace

// static
std::unique_ptr<TabSharingUI> TabSharingUI::Create(
    const content::DesktopMediaID& media_id,
    base::string16 app_name) {
  return base::WrapUnique(new TabSharingUIViews(media_id, app_name));
}

TabSharingUIViews::TabSharingUIViews(const content::DesktopMediaID& media_id,
                                     base::string16 app_name)
    : app_name_(std::move(app_name)) {
  shared_tab_ = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(
          media_id.web_contents_id.render_process_id,
          media_id.web_contents_id.main_render_frame_id));
  shared_tab_name_ = GetTabName(shared_tab_);
  profile_ = ProfileManager::GetLastUsedProfileAllowedByPolicy();
#if !defined(OS_MACOSX)
  // TODO(https://crbug.com/991896) fix contents border on Mac.
  InitContentsBorderWidget(shared_tab_);
#endif
}

TabSharingUIViews::~TabSharingUIViews() {
  if (!infobars_.empty())
    StopSharing();
}

gfx::NativeViewId TabSharingUIViews::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback) {
  source_callback_ = std::move(source_callback);
  stop_callback_ = std::move(stop_callback);
  CreateInfobarsForAllTabs();
  SetContentsBorderVisible(shared_tab_, true);
  return 0;
}

void TabSharingUIViews::StartSharing(infobars::InfoBar* infobar) {
  if (source_callback_.is_null())
    return;

  SetContentsBorderVisible(shared_tab_, false);

  content::WebContents* shared_tab =
      InfoBarService::WebContentsFromInfoBar(infobar);
  DCHECK(shared_tab);
  DCHECK_EQ(infobars_[shared_tab], infobar);
  shared_tab_ = shared_tab;
  shared_tab_name_ = GetTabName(shared_tab_);

  content::RenderFrameHost* main_frame = shared_tab->GetMainFrame();
  DCHECK(main_frame);
  RemoveInfobarsForAllTabs();
  source_callback_.Run(content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                         main_frame->GetRoutingID())));
}

void TabSharingUIViews::StopSharing() {
  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();
  RemoveInfobarsForAllTabs();
  SetContentsBorderVisible(shared_tab_, false);
  shared_tab_ = nullptr;
}

void TabSharingUIViews::OnBrowserAdded(Browser* browser) {
  if (browser->profile()->GetOriginalProfile() == profile_)
    tab_strip_models_observer_.Add(browser->tab_strip_model());
}

void TabSharingUIViews::OnBrowserRemoved(Browser* browser) {
  BrowserList* browser_list = BrowserList::GetInstance();
  if (browser_list->empty())
    browser_list->RemoveObserver(this);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  if (tab_strip_models_observer_.IsObserving(tab_strip_model))
    tab_strip_models_observer_.Remove(tab_strip_model);
}

void TabSharingUIViews::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (infobars_.find(contents.contents) == infobars_.end())
        CreateInfobarForWebContents(contents.contents);
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    auto* remove = change.GetRemove();
    if (remove->will_be_deleted) {
      bool remove_shared_tab = false;
      for (const auto& contents : remove->contents) {
        remove_shared_tab |= contents.contents == shared_tab_;
        infobars_.erase(contents.contents);
      }
      if (remove_shared_tab)
        StopSharing();
    }
  }

  if (selection.active_tab_changed()) {
    if (selection.old_contents)
      SetContentsBorderVisible(selection.old_contents,
                               selection.old_contents == shared_tab_);
    SetContentsBorderVisible(selection.new_contents,
                             selection.new_contents == shared_tab_);
  }
}

void TabSharingUIViews::CreateInfobarsForAllTabs() {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (auto* browser : *browser_list) {
    OnBrowserAdded(browser);

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      CreateInfobarForWebContents(tab_strip_model->GetWebContentsAt(i));
    }
  }
  browser_list->AddObserver(this);
}

void TabSharingUIViews::CreateInfobarForWebContents(
    content::WebContents* contents) {
  infobars_[contents] = TabSharingInfoBarDelegate::Create(
      InfoBarService::FromWebContents(contents),
      shared_tab_ == contents ? base::string16() : shared_tab_name_, app_name_,
      !source_callback_.is_null() /*is_sharing_allowed*/, this);
}

void TabSharingUIViews::RemoveInfobarsForAllTabs() {
  BrowserList::GetInstance()->RemoveObserver(this);
  tab_strip_models_observer_.RemoveAll();

  for (const auto& infobars_entry : infobars_)
    infobars_entry.second->RemoveSelf();

  infobars_.clear();
}
