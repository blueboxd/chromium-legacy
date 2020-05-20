// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/reauth_tab_helper.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace {

const int kModalDialogWidth = 448;
const int kSyncConfirmationDialogWidth = 512;
const int kSyncConfirmationDialogHeight = 487;
const int kSigninErrorDialogHeight = 164;
const int kReauthDialogWidth = 540;
const int kReauthDialogHeight = 520;

int GetSyncConfirmationDialogPreferredHeight(Profile* profile) {
  // If sync is disabled, then the sync confirmation dialog looks like an error
  // dialog and thus it has the same preferred size.
  return ProfileSyncServiceFactory::IsSyncAllowed(profile)
             ? kSyncConfirmationDialogHeight
             : kSigninErrorDialogHeight;
}

void CompleteReauthFlow(Browser* browser,
                        base::OnceCallback<void(signin::ReauthResult)> callback,
                        signin::ReauthResult result) {
  browser->signin_view_controller()->CloseModalSignin();
  std::move(callback).Run(result);
}
}  // namespace

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
    Browser* browser) {
  return CreateDialogWebView(
      browser, chrome::kChromeUISyncConfirmationURL,
      GetSyncConfirmationDialogPreferredHeight(browser->profile()),
      kSyncConfirmationDialogWidth);
}

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateSigninErrorWebView(Browser* browser) {
  return CreateDialogWebView(browser, chrome::kChromeUISigninErrorURL,
                             kSigninErrorDialogHeight, base::nullopt);
}

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateReauthConfirmationWebView(
    Browser* browser) {
  return CreateDialogWebView(browser, chrome::kChromeUISigninReauthURL,
                             kReauthDialogHeight, kReauthDialogWidth);
}

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateGaiaReauthWebView(
    Browser* browser,
    base::OnceCallback<void(signin::ReauthResult)> reauth_callback) {
  auto web_view = std::make_unique<views::WebView>(browser->profile());
  const GURL& reauth_url = GaiaUrls::GetInstance()->reauth_url();
  web_view->LoadInitialURL(reauth_url);

  gfx::Size max_size = browser->window()
                           ->GetWebContentsModalDialogHost()
                           ->GetMaximumDialogSize();
  web_view->SetPreferredSize(
      gfx::Size(std::min(kReauthDialogWidth, max_size.width()),
                std::min(kReauthDialogHeight, max_size.height())));

  content::WebContents* web_contents = web_view->GetWebContents();

  signin::ReauthTabHelper::CreateForWebContents(
      web_contents, reauth_url, true,
      base::BindOnce(&CompleteReauthFlow, base::Unretained(browser),
                     std::move(reauth_callback)));

  return web_view;
}

views::View* SigninViewControllerDelegateViews::GetContentsView() {
  return content_view_;
}

views::Widget* SigninViewControllerDelegateViews::GetWidget() {
  return content_view_->GetWidget();
}

const views::Widget* SigninViewControllerDelegateViews::GetWidget() const {
  return content_view_->GetWidget();
}

void SigninViewControllerDelegateViews::DeleteDelegate() {
  NotifyModalSigninClosed();
  delete this;
}

ui::ModalType SigninViewControllerDelegateViews::GetModalType() const {
  return dialog_modal_type_;
}

bool SigninViewControllerDelegateViews::ShouldShowCloseButton() const {
  return should_show_close_button_;
}

void SigninViewControllerDelegateViews::CloseModalSignin() {
  signin::ReauthTabHelper* reauth_tab_helper =
      signin::ReauthTabHelper::FromWebContents(web_contents_);
  if (reauth_tab_helper) {
    reauth_tab_helper->CompleteReauth(signin::ReauthResult::kCancelled);
  }

  NotifyModalSigninClosed();
  if (modal_signin_widget_)
    modal_signin_widget_->Close();
}

void SigninViewControllerDelegateViews::ResizeNativeView(int height) {
  int max_height = browser()
                       ->window()
                       ->GetWebContentsModalDialogHost()
                       ->GetMaximumDialogSize()
                       .height();
  content_view_->SetPreferredSize(gfx::Size(
      content_view_->GetPreferredSize().width(), std::min(height, max_height)));

  if (!modal_signin_widget_) {
    // The modal wasn't displayed yet so just show it with the already resized
    // view.
    DisplayModal();
  }
}

content::WebContents* SigninViewControllerDelegateViews::GetWebContents() {
  return web_contents_;
}

bool SigninViewControllerDelegateViews::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Discard the context menu
  return true;
}

bool SigninViewControllerDelegateViews::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // If this is a MODAL_TYPE_CHILD, then GetFocusManager() will return the focus
  // manager of the parent window, which has registered accelerators, and the
  // accelerators will fire. If this is a MODAL_TYPE_WINDOW, then this will have
  // no effect, since no accelerators have been registered for this standalone
  // window.
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

web_modal::WebContentsModalDialogHost*
SigninViewControllerDelegateViews::GetWebContentsModalDialogHost() {
  return browser()->window()->GetWebContentsModalDialogHost();
}

SigninViewControllerDelegateViews::SigninViewControllerDelegateViews(
    std::unique_ptr<views::WebView> content_view,
    Browser* browser,
    ui::ModalType dialog_modal_type,
    bool wait_for_size,
    bool should_show_close_button)
    : web_contents_(content_view->GetWebContents()),
      browser_(browser),
      content_view_(content_view.release()),
      modal_signin_widget_(nullptr),
      dialog_modal_type_(dialog_modal_type),
      should_show_close_button_(should_show_close_button) {
  DCHECK(web_contents_);
  DCHECK(browser_);
  DCHECK(browser_->tab_strip_model()->GetActiveWebContents())
      << "A tab must be active to present the sign-in modal dialog.";

  SetButtons(ui::DIALOG_BUTTON_NONE);

  web_contents_->SetDelegate(this);

  DCHECK(dialog_modal_type == ui::MODAL_TYPE_CHILD ||
         dialog_modal_type == ui::MODAL_TYPE_WINDOW)
      << "Unsupported dialog modal type " << dialog_modal_type;

  if (!wait_for_size)
    DisplayModal();
}

SigninViewControllerDelegateViews::~SigninViewControllerDelegateViews() =
    default;

std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateDialogWebView(
    Browser* browser,
    const std::string& url,
    int dialog_height,
    base::Optional<int> opt_width) {
  int dialog_width = opt_width.value_or(kModalDialogWidth);
  views::WebView* web_view = new views::WebView(browser->profile());
  web_view->LoadInitialURL(GURL(url));
  // To record metrics using javascript, extensions are needed.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view->GetWebContents());

  SigninWebDialogUI* web_dialog_ui = static_cast<SigninWebDialogUI*>(
      web_view->GetWebContents()->GetWebUI()->GetController());
  web_dialog_ui->InitializeMessageHandlerWithBrowser(browser);

  int max_height = browser->window()
                       ->GetWebContentsModalDialogHost()
                       ->GetMaximumDialogSize()
                       .height();
  web_view->SetPreferredSize(
      gfx::Size(dialog_width, std::min(dialog_height, max_height)));

  return std::unique_ptr<views::WebView>(web_view);
}

void SigninViewControllerDelegateViews::DisplayModal() {
  DCHECK(!modal_signin_widget_);

  content::WebContents* host_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Avoid displaying the sign-in modal view if there are no active web
  // contents. This happens if the user closes the browser window before this
  // dialog has a chance to be displayed.
  if (!host_web_contents)
    return;

  gfx::NativeWindow window = host_web_contents->GetTopLevelNativeWindow();
  switch (dialog_modal_type_) {
    case ui::MODAL_TYPE_WINDOW:
      modal_signin_widget_ =
          constrained_window::CreateBrowserModalDialogViews(this, window);
      modal_signin_widget_->Show();
      break;
    case ui::MODAL_TYPE_CHILD:
      modal_signin_widget_ = constrained_window::ShowWebModalDialogViews(
          this, browser()->tab_strip_model()->GetActiveWebContents());
      break;
    default:
      NOTREACHED() << "Unsupported dialog modal type " << dialog_modal_type_;
  }
  if (should_show_close_button_) {
    GetBubbleFrameView()->SetBubbleBorder(std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::NONE, views::BubbleBorder::BIG_SHADOW,
        SK_ColorWHITE));
  }

  content_view_->RequestFocus();
}

// --------------------------------------------------------------------
// SigninViewControllerDelegate static methods
// --------------------------------------------------------------------

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSyncConfirmationDelegate(Browser* browser) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(browser),
      browser, ui::MODAL_TYPE_WINDOW, true, false);
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSigninErrorDelegate(Browser* browser) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateSigninErrorWebView(browser),
      browser, ui::MODAL_TYPE_WINDOW, true, false);
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateGaiaReauthDelegate(
    Browser* browser,
    const CoreAccountId& account_id,
    base::OnceCallback<void(signin::ReauthResult)> reauth_callback) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateGaiaReauthWebView(
          browser, std::move(reauth_callback)),
      browser, ui::MODAL_TYPE_CHILD, false, true);
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateReauthConfirmationDelegate(
    Browser* browser,
    const CoreAccountId& account_id) {
  return new SigninViewControllerDelegateViews(
      SigninViewControllerDelegateViews::CreateReauthConfirmationWebView(
          browser),
      browser, ui::MODAL_TYPE_CHILD, false, true);
}
