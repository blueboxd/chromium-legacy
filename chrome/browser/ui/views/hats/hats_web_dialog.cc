// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hats/hats_web_dialog.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace {

// Default width/height of the dialog in screen size.
const int kDefaultWidth = 400;
const int kDefaultHeight = 420;

// Site ID of HaTS survey for Chrome Desktop.
// TODO(weili): Replace this with the pilot survey site ID retrieved from finch
// config.
constexpr char kSiteID[] = "z4cctguzopq5x2ftal6vdgjrui";

// Placeholder strings in html file to be replaced when the file is loaded.
constexpr char kScriptSrcReplacementToken[] = "$SCRIPT_SRC";
constexpr char kDoneButtonLabelReplacementToken[] = "$DONE_BUTTON_LABEL";

// Base URL to fetch the Google consumer survey script.
constexpr char kBaseFormatUrl[] =
    "https://www.google.com/insights/consumersurveys/"
    "async_survey?site=%s&force_https=1&sc=%s";

// Returns the local HaTS HTML file as a string with the correct Hats script
// URL.
// |site_id| refers to the 'Site ID' used by HaTS server to uniquely identify
// a survey to be served.
// |site_context| is the extra information that may be sent along with the
// survey feedback to the HaTS server.
std::string LoadLocalHtmlAsString(const std::string& site_id,
                                  const std::string& site_context) {
  std::string html_data;
  ui::ResourceBundle::GetSharedInstance()
      .GetRawDataResource(IDR_DESKTOP_HATS_HTML)
      .CopyToString(&html_data);

  size_t pos = html_data.find(kScriptSrcReplacementToken);
  DCHECK(pos != std::string::npos);
  html_data.replace(pos, strlen(kScriptSrcReplacementToken),
                    base::StringPrintf(kBaseFormatUrl, site_id.c_str(),
                                       site_context.c_str()));

  pos = html_data.find(kDoneButtonLabelReplacementToken);
  DCHECK(pos != std::string::npos);
  html_data.replace(pos, strlen(kDoneButtonLabelReplacementToken),
                    l10n_util::GetStringUTF8(IDS_DONE));

  return html_data;
}

}  // namespace

// static
void HatsWebDialog::Show(const Browser* browser) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(browser);

  // Self deleting upon close.
  auto* hats_dialog = new HatsWebDialog();

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  chrome::ShowWebDialog(browser_view->GetWidget()->GetNativeView(),
                        browser->profile(), hats_dialog);
}

HatsWebDialog::HatsWebDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

HatsWebDialog::~HatsWebDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ui::ModalType HatsWebDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 HatsWebDialog::GetDialogTitle() const {
  return base::string16();
}

GURL HatsWebDialog::GetDialogContentURL() const {
  // Load the html data and use it in a data URL to be displayed in the dialog.
  std::string url_str =
      LoadLocalHtmlAsString(kSiteID, version_info::GetVersionNumber());
  url::RawCanonOutputT<char> url;
  url::EncodeURIComponent(url_str.c_str(), url_str.length(), &url);
  return GURL("data:text/html;charset=utf-8," +
              std::string(url.data(), url.length()));
}

void HatsWebDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void HatsWebDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

bool HatsWebDialog::CanResizeDialog() const {
  return false;
}

std::string HatsWebDialog::GetDialogArgs() const {
  return std::string();
}

void HatsWebDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void HatsWebDialog::OnCloseContents(content::WebContents* source,
                                    bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HatsWebDialog::ShouldShowDialogTitle() const {
  return false;
}

bool HatsWebDialog::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}
