// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_webui_impl.h"

namespace media_router {

// static
MediaRouterDialogControllerImplBase*
MediaRouterDialogControllerImplBase::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
    return MediaRouterDialogControllerViews::GetOrCreateForWebContents(
        web_contents);
}

// static
MediaRouterDialogControllerImplBase*
MediaRouterDialogControllerImplBase::FromWebContents(
    content::WebContents* web_contents) {
    return MediaRouterDialogControllerViews::FromWebContents(web_contents);
}

MediaRouterDialogControllerViews::~MediaRouterDialogControllerViews() {
  Reset();
  if (dialog_widget_) {
    dialog_widget_->RemoveObserver(this);
    dialog_widget_ = nullptr;
  }
}

// static
MediaRouterDialogControllerViews*
MediaRouterDialogControllerViews::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // This call does nothing if the controller already exists.
  CreateForWebContents(web_contents);
  return FromWebContents(web_contents);
}

void MediaRouterDialogControllerViews::CreateMediaRouterDialog() {
  base::Time dialog_creation_time = base::Time::Now();
  MediaRouterDialogControllerImplBase::CreateMediaRouterDialog();

  Profile* profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());
  ui_ = std::make_unique<MediaRouterViewsUI>();
  InitializeMediaRouterUI(ui_.get());

  Browser* browser = chrome::FindBrowserWithWebContents(initiator());
  if (browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (browser_view->toolbar()->cast_button()) {
      CastDialogView::ShowDialogWithToolbarAction(ui_.get(), browser,
                                                  dialog_creation_time);
    } else {
      CastDialogView::ShowDialogCenteredForBrowserWindow(ui_.get(), browser,
                                                         dialog_creation_time);
    }
  } else {
    gfx::Rect anchor_bounds = initiator()->GetContainerBounds();
    // Set the height to 0 so that the dialog gets anchored to the top of the
    // window.
    anchor_bounds.set_height(0);
    CastDialogView::ShowDialogCentered(anchor_bounds, ui_.get(), profile,
                                       dialog_creation_time);
  }
  dialog_widget_ = CastDialogView::GetCurrentDialogWidget();
  dialog_widget_->AddObserver(this);
  if (dialog_creation_callback_)
    dialog_creation_callback_.Run();
}

void MediaRouterDialogControllerViews::CloseMediaRouterDialog() {
  CastDialogView::HideDialog();
}

bool MediaRouterDialogControllerViews::IsShowingMediaRouterDialog() const {
  return CastDialogView::IsShowing();
}

void MediaRouterDialogControllerViews::Reset() {
  // If |ui_| is null, Reset() has already been called.
  if (ui_) {
    MediaRouterDialogControllerImplBase::Reset();
    ui_.reset();
  }
}

void MediaRouterDialogControllerViews::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(dialog_widget_, widget);
  Reset();
  dialog_widget_->RemoveObserver(this);
  dialog_widget_ = nullptr;
}

void MediaRouterDialogControllerViews::SetDialogCreationCallbackForTesting(
    base::RepeatingClosure callback) {
  dialog_creation_callback_ = std::move(callback);
}

MediaRouterDialogControllerViews::MediaRouterDialogControllerViews(
    content::WebContents* web_contents)
    : MediaRouterDialogControllerImplBase(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaRouterDialogControllerViews)

}  // namespace media_router
