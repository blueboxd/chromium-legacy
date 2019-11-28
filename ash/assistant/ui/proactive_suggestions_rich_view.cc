// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/proactive_suggestions_rich_view.h"

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "base/base64.h"
#include "chromeos/services/assistant/public/features.h"
#include "ui/aura/window.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

ProactiveSuggestionsRichView::ProactiveSuggestionsRichView(
    AssistantViewDelegate* delegate)
    : ProactiveSuggestionsView(delegate) {}

ProactiveSuggestionsRichView::~ProactiveSuggestionsRichView() {
  if (contents_)
    contents_->RemoveObserver(this);
}

const char* ProactiveSuggestionsRichView::GetClassName() const {
  return "ProactiveSuggestionsRichView";
}

void ProactiveSuggestionsRichView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Initialize NavigableContentsFactory.
  delegate()->GetNavigableContentsFactoryForView(
      contents_factory_.BindNewPipeAndPassReceiver());

  // Initialize NavigableContentsParams.
  auto params = content::mojom::NavigableContentsParams::New();
  params->enable_view_auto_resize = true;
  params->auto_resize_min_size = gfx::Size(1, 1);
  params->auto_resize_max_size = gfx::Size(INT_MAX, INT_MAX);
  params->suppress_navigations = true;

  // Initialize NavigableContents.
  contents_ = std::make_unique<content::NavigableContents>(
      contents_factory_.get(), std::move(params));
  contents_->AddObserver(this);

  // TODO(dmblack): Replace w/ server provided HTML.
  std::string encoded_html;
  base::Base64Encode(R"(
    <!DOCTYPE html>
    <html>
      <body>
        <style>
          * {
            box-sizing: border-box;
            cursor: default;
            margin: 0;
            padding: 0;
            text-decoration: none;
            user-select: none;
          }
          body {
            align-items: center;
            background-color: #3c4043;
            display: flex;
            height: 32px;
            max-width: 280px;
            overflow: hidden;
          }
          .EntryPoint {
            align-items: center;
            display: flex;
            flex: 1 1 auto;
            height: 100%;
            overflow: hidden;
          }
          .Icon {
            background-image:
              url("https://www.gstatic.com/images/branding/product/2x/assistant_24dp.png");
            background-position: center;
            background-repeat: no-repeat;
            background-size: 100%;
            flex: 0 0 auto;
            height: 16px;
            margin-left: 8px;
            width: 16px;
          }
          .Label {
            color: #f1f3f4;
            flex: 1 1 auto;
            font-family: Google Sans, sans-serif;
            font-size: 13px;
            font-weight: normal;
            line-height: 20px;
            margin-left: 8px;
            overflow: hidden;
            white-space: nowrap;
          }
          .Close {
            background-color: #f1f3f4;
            flex: 0 0 auto;
            height: 32px;
            width: 32px;
            -webkit-mask-image:
              url("https://www.gstatic.com/images/icons/material/system/2x/close_black_24dp.png");
            -webkit-mask-position: center;
            -webkit-mask-repeat: no-repeat;
            -webkit-mask-size: 16px;
          }
        </style>
        <a class="EntryPoint" href="googleassistant://proactive-suggestions?action=entryPointClick">
          <div class="Icon"></div>
          <p class="Label">Related pages</p>
        </a>
        <a href="googleassistant://proactive-suggestions?action=entryPointClose">
          <div class="Close"></div>
        </a>
      </body>
    </html>
    )",
                     &encoded_html);

  // Navigate to the data URL representing our encoded HTML.
  constexpr char kDataUriPrefix[] = "data:text/html;base64,";
  contents_->Navigate(GURL(kDataUriPrefix + encoded_html));
}

void ProactiveSuggestionsRichView::ShowWhenReady() {
  // If no children have yet been added to the layout, the embedded web contents
  // has not yet finished loading. In this case, we'll set a flag and delay
  // showing until loading stops to avoid UI jank.
  if (children().empty()) {
    show_when_ready_ = true;
    return;
  }
  // Otherwise it's fine to go ahead w/ showing the widget immediately.
  GetWidget()->ShowInactive();
  show_when_ready_ = false;
}

void ProactiveSuggestionsRichView::Hide() {
  show_when_ready_ = false;
  ProactiveSuggestionsView::Hide();
}

void ProactiveSuggestionsRichView::Close() {
  show_when_ready_ = false;
  ProactiveSuggestionsView::Close();
}

void ProactiveSuggestionsRichView::DidAutoResizeView(
    const gfx::Size& new_size) {
  contents_->GetView()->view()->SetPreferredSize(new_size);
  PreferredSizeChanged();
}

void ProactiveSuggestionsRichView::DidStopLoading() {
  AddChildView(contents_->GetView()->view());
  PreferredSizeChanged();

  // Once the view for the embedded web contents has been fully initialized,
  // it's safe to set our desired corner radius.
  contents_->GetView()->native_view()->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(
          chromeos::assistant::features::
              GetProactiveSuggestionsRichEntryPointCornerRadius()));

  // This view should now be fully initialized, so it's safe to show without
  // risk of introducing UI jank if we so desire.
  if (show_when_ready_)
    ShowWhenReady();
}

void ProactiveSuggestionsRichView::DidSuppressNavigation(
    const GURL& url,
    WindowOpenDisposition disposition,
    bool from_user_gesture) {
  if (from_user_gesture)
    delegate()->OpenUrlFromView(url);
}

}  // namespace ash
