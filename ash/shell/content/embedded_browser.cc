// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/embedded_browser.h"

#include "ash/public/cpp/app_types.h"
#include "ash/shell.h"
#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace ash {
namespace shell {

namespace {

// Simulate a widget created on Window Service side to host a native window.
// For classic, it is done by NativeViewHost. For single process mash,
// it uses ServerRemoteViewHost. To implement for mash, the code should be
// moved to ash process somehow and invoked over mojo.
class HostWidget : public views::WidgetDelegateView {
 public:
  // Creates HostWidget that hosts |native_window| directly for classic.
  static void CreateFromNativeWindow(gfx::NativeWindow native_window) {
    // HostWidget deletes itself when underlying widget closes.
    new HostWidget(native_window);
  }

 private:
  HostWidget(gfx::NativeWindow native_window) {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    auto* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.context = Shell::GetPrimaryRootWindow();
    params.bounds = gfx::Rect(20, 0, 800, 600);
    params.delegate = this;
    widget->Init(params);
    widget->Show();

    auto* host = new views::NativeViewHost();
    AddChildView(host);
    host->Attach(native_window);

    Layout();
  }

  // views::WidgetDelegateView:
  base::string16 GetWindowTitle() const override {
    const static base::string16 title =
        base::ASCIIToUTF16("Classic: Embed by NativeViewHost");
    return title;
  }

  DISALLOW_COPY_AND_ASSIGN(HostWidget);
};

}  // namespace

EmbeddedBrowser::EmbeddedBrowser(content::BrowserContext* context,
                                 const GURL& url) {
  contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(context));
  contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
  contents_->GetNativeView()->Show();

  HostWidget::CreateFromNativeWindow(contents_->GetNativeView());
}

EmbeddedBrowser::~EmbeddedBrowser() = default;

aura::Window* EmbeddedBrowser::GetWindow() {
  return contents_->GetNativeView()->GetToplevelWindow();
}

// static
aura::Window* EmbeddedBrowser::Create(content::BrowserContext* context,
                                      const GURL& url) {
  // EmbeddedBrowser deletes itself when the hosting widget is closed.
  // TODO(oshima): This leaks site process map for some reason. Fix it.
  // crbug.com/983397.
  aura::Window* browser_window =
      (new EmbeddedBrowser(context, url))->GetWindow();
  browser_window->SetProperty(aura::client::kAppType,
                              static_cast<int>(ash::AppType::BROWSER));
  return browser_window;
}

void EmbeddedBrowser::OnUnembed() {
  delete this;
}

}  // namespace shell
}  // namespace ash
