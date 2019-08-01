// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/chrome_colors/chrome_colors_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/search/search.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

bool IsCacheableNTP(content::WebContents* contents) {
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  return search::NavEntryIsInstantNTP(contents, entry) &&
         entry->GetURL() != chrome::kChromeSearchLocalNtpUrl;
}

// Returns true if |contents| are rendered inside an Instant process.
bool InInstantProcess(const InstantService* instant_service,
                      content::WebContents* contents) {
  if (!instant_service || !contents)
    return false;

  return instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID());
}

// Called when an NTP finishes loading. If the load start time was noted,
// calculates and logs the total load time.
void RecordNewTabLoadTime(content::WebContents* contents) {
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(contents);
  if (core_tab_helper->new_tab_start_time().is_null())
    return;

  base::TimeDelta duration =
      base::TimeTicks::Now() - core_tab_helper->new_tab_start_time();
  if (IsCacheableNTP(contents)) {
    if (google_util::IsGoogleDomainUrl(
            contents->GetController().GetLastCommittedEntry()->GetURL(),
            google_util::ALLOW_SUBDOMAIN,
            google_util::DISALLOW_NON_STANDARD_PORTS)) {
      UMA_HISTOGRAM_TIMES("Tab.NewTabOnload.Google", duration);
    } else {
      UMA_HISTOGRAM_TIMES("Tab.NewTabOnload.Other", duration);
    }
  } else {
    UMA_HISTOGRAM_TIMES("Tab.NewTabOnload.Local", duration);
  }
  core_tab_helper->set_new_tab_start_time(base::TimeTicks());
}

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      ipc_router_(web_contents,
                  this,
                  std::make_unique<SearchIPCRouterPolicyImpl>(web_contents)),
      instant_service_(nullptr) {
  DCHECK(search::IsInstantExtendedAPIEnabled());

  instant_service_ = InstantServiceFactory::GetForProfile(profile());
  if (instant_service_)
    instant_service_->AddObserver(this);

  search_suggest_service_ =
      SearchSuggestServiceFactory::GetForProfile(profile());

  chrome_colors_service_ =
      chrome_colors::ChromeColorsFactory::GetForProfile(profile());
}

SearchTabHelper::~SearchTabHelper() {
  if (instant_service_)
    instant_service_->RemoveObserver(this);
}

void SearchTabHelper::OmniboxInputStateChanged() {
  ipc_router_.SetInputInProgress(IsInputInProgress());
}

void SearchTabHelper::OmniboxFocusChanged(OmniboxFocusState state,
                                          OmniboxFocusChangeReason reason) {
  ipc_router_.OmniboxFocusChanged(state, reason);

  // Don't send oninputstart/oninputend updates in response to focus changes
  // if there's a navigation in progress. This prevents Chrome from sending
  // a spurious oninputend when the user accepts a match in the omnibox.
  if (web_contents_->GetController().GetPendingEntry() == nullptr)
    ipc_router_.SetInputInProgress(IsInputInProgress());
}

void SearchTabHelper::OnTabActivated() {
  ipc_router_.OnTabActivated();

  if (search::IsInstantNTP(web_contents_)) {
    if (instant_service_)
      instant_service_->OnNewTabPageOpened();

    // Force creation of NTPUserDataLogger, if we loaded an NTP. The
    // NTPUserDataLogger tries to detect whether the NTP is being created at
    // startup or from the user opening a new tab, and if we wait until later,
    // it won't correctly detect this case.
    NTPUserDataLogger::GetOrCreateFromWebContents(web_contents_);
  }
}

void SearchTabHelper::OnTabDeactivated() {
  ipc_router_.OnTabDeactivated();
}

void SearchTabHelper::OnTabClosing() {
  if (search::IsInstantNTP(web_contents_) && chrome_colors_service_)
    chrome_colors_service_->RevertThemeChangesForTab(web_contents_);
}

void SearchTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (search::IsNTPOrRelatedURL(navigation_handle->GetURL(), profile())) {
    // Set the title on any pending entry corresponding to the NTP. This
    // prevents any flickering of the tab title.
    content::NavigationEntry* entry =
        web_contents_->GetController().GetPendingEntry();
    if (entry) {
      web_contents_->UpdateTitleForEntry(
          entry, l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
    }
  }
}

void SearchTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  if (is_setting_title_ || !entry)
    return;

  // Always set the title on the new tab page to be the one from our UI
  // resources. This check ensures that the title is properly set to the string
  // defined by the Chrome UI language (rather than the server language) in all
  // cases.
  //
  // We only override the title when it's nonempty to allow the page to set the
  // title if it really wants. An empty title means to use the default. There's
  // also a race condition between this code and the page's SetTitle call which
  // this rule avoids.
  if (entry->GetTitle().empty() &&
      search::NavEntryIsInstantNTP(web_contents_, entry)) {
    is_setting_title_ = true;
    web_contents_->UpdateTitleForEntry(
        entry, l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
    is_setting_title_ = false;
  }
}

void SearchTabHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& /* validated_url */) {
  if (!render_frame_host->GetParent() && search::IsInstantNTP(web_contents_))
    RecordNewTabLoadTime(web_contents_);
}

void SearchTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.is_main_frame)
    return;

  if (search::IsInstantNTP(web_contents_))
    ipc_router_.SetInputInProgress(IsInputInProgress());

  if (InInstantProcess(instant_service_, web_contents_))
    ipc_router_.OnNavigationEntryCommitted();
}

void SearchTabHelper::ThemeInfoChanged(const ThemeBackgroundInfo& theme_info) {
  ipc_router_.SendThemeBackgroundInfo(theme_info);
}

void SearchTabHelper::MostVisitedInfoChanged(
    const InstantMostVisitedInfo& most_visited_info) {
  ipc_router_.SendMostVisitedInfo(most_visited_info);
}

void SearchTabHelper::FocusOmnibox(bool focus) {
  OmniboxView* omnibox_view = GetOmniboxView();
  if (!omnibox_view)
    return;

  if (focus) {
    omnibox_view->SetFocus();
    omnibox_view->model()->SetCaretVisibility(false);
    // If the user clicked on the fakebox, any text already in the omnibox
    // should get cleared when they start typing. Selecting all the existing
    // text is a convenient way to accomplish this. It also gives a slight
    // visual cue to users who really understand selection state about what
    // will happen if they start typing.
    omnibox_view->SelectAll(false);
#if !defined(OS_WIN)
    omnibox_view->ShowVirtualKeyboardIfEnabled();
#endif
  } else {
    // Remove focus only if the popup is closed. This will prevent someone
    // from changing the omnibox value and closing the popup without user
    // interaction.
    if (!omnibox_view->model()->popup_model()->IsOpen())
      web_contents()->Focus();
  }
}

void SearchTabHelper::OnDeleteMostVisitedItem(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    instant_service_->DeleteMostVisitedItem(url);
}

void SearchTabHelper::OnUndoMostVisitedDeletion(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    instant_service_->UndoMostVisitedDeletion(url);
}

void SearchTabHelper::OnUndoAllMostVisitedDeletions() {
  if (instant_service_)
    instant_service_->UndoAllMostVisitedDeletions();
}

bool SearchTabHelper::OnAddCustomLink(const GURL& url,
                                      const std::string& title) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    return instant_service_->AddCustomLink(url, title);
  return false;
}

bool SearchTabHelper::OnUpdateCustomLink(const GURL& url,
                                         const GURL& new_url,
                                         const std::string& new_title) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    return instant_service_->UpdateCustomLink(url, new_url, new_title);
  return false;
}

bool SearchTabHelper::OnReorderCustomLink(const GURL& url, int new_pos) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    return instant_service_->ReorderCustomLink(url, new_pos);
  return false;
}

bool SearchTabHelper::OnDeleteCustomLink(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    return instant_service_->DeleteCustomLink(url);
  return false;
}

void SearchTabHelper::OnUndoCustomLinkAction() {
  if (instant_service_)
    instant_service_->UndoCustomLinkAction();
}

void SearchTabHelper::OnResetCustomLinks() {
  if (instant_service_)
    instant_service_->ResetCustomLinks();
}

void SearchTabHelper::OnToggleMostVisitedOrCustomLinks() {
  if (instant_service_)
    instant_service_->ToggleMostVisitedOrCustomLinks();
}

void SearchTabHelper::OnToggleShortcutsVisibility(bool do_notify) {
  if (instant_service_)
    instant_service_->ToggleShortcutsVisibility(do_notify);
}

void SearchTabHelper::OnLogEvent(NTPLoggingEventType event,
                                 base::TimeDelta time) {
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(event, time);
}

void SearchTabHelper::OnLogSuggestionEventWithValue(
    NTPSuggestionsLoggingEventType event,
    int data,
    base::TimeDelta time) {
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogSuggestionEventWithValue(event, data, time);
}

void SearchTabHelper::OnLogMostVisitedImpression(
    const ntp_tiles::NTPTileImpression& impression) {
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogMostVisitedImpression(impression);
}

void SearchTabHelper::OnLogMostVisitedNavigation(
    const ntp_tiles::NTPTileImpression& impression) {
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogMostVisitedNavigation(impression);
}

void SearchTabHelper::PasteIntoOmnibox(const base::string16& text) {
  OmniboxView* omnibox_view = GetOmniboxView();
  if (!omnibox_view)
    return;
  // The first case is for right click to paste, where the text is retrieved
  // from the clipboard already sanitized. The second case is needed to handle
  // drag-and-drop value and it has to be sanitazed before setting it into the
  // omnibox.
  base::string16 text_to_paste = text.empty()
                                     ? GetClipboardText()
                                     : omnibox_view->SanitizeTextForPaste(text);

  if (text_to_paste.empty())
    return;

  if (!omnibox_view->model()->has_focus())
    omnibox_view->SetFocus();

  omnibox_view->OnBeforePossibleChange();
  omnibox_view->model()->OnPaste();
  omnibox_view->SetUserText(text_to_paste);
  omnibox_view->OnAfterPossibleChange(true);
}

void SearchTabHelper::OnSetCustomBackgroundInfo(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::string& collection_id) {
  if (instant_service_) {
    instant_service_->SetCustomBackgroundInfo(
        background_url, attribution_line_1, attribution_line_2, action_url,
        collection_id);
  }
}

void SearchTabHelper::FileSelected(const base::FilePath& path,
                                   int index,
                                   void* params) {
  if (instant_service_) {
    profile()->set_last_selected_directory(path.DirName());
    instant_service_->SelectLocalBackgroundImage(path);
  }

  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_DONE,
                 base::TimeDelta::FromSeconds(0));
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(NTP_BACKGROUND_UPLOAD_DONE, base::TimeDelta::FromSeconds(0));

  ipc_router_.SendLocalBackgroundSelected();
}

void SearchTabHelper::FileSelectionCanceled(void* params) {
  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL,
                 base::TimeDelta::FromSeconds(0));
  NTPUserDataLogger::GetOrCreateFromWebContents(web_contents())
      ->LogEvent(NTP_BACKGROUND_UPLOAD_CANCEL, base::TimeDelta::FromSeconds(0));
}

void SearchTabHelper::OnSelectLocalBackgroundImage() {
  if (select_file_dialog_)
    return;

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents_));

  const base::FilePath directory = profile()->last_selected_directory();

  gfx::NativeWindow parent_window = web_contents_->GetTopLevelNativeWindow();

  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  file_types.extensions.resize(1);
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("png"));
  file_types.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_UPLOAD_IMAGE_FORMAT));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::string16(), directory,
      &file_types, 0, base::FilePath::StringType(), parent_window, nullptr);
}

const OmniboxView* SearchTabHelper::GetOmniboxView() const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser)
    return nullptr;

  return browser->window()->GetLocationBar()->GetOmniboxView();
}

void SearchTabHelper::OnBlocklistSearchSuggestion(int task_version,
                                                  long task_id) {
  if (search_suggest_service_)
    search_suggest_service_->BlocklistSearchSuggestion(task_version, task_id);
}

void SearchTabHelper::OnBlocklistSearchSuggestionWithHash(
    int task_version,
    long task_id,
    const uint8_t hash[4]) {
  if (search_suggest_service_)
    search_suggest_service_->BlocklistSearchSuggestionWithHash(task_version,
                                                               task_id, hash);
}

void SearchTabHelper::OnSearchSuggestionSelected(int task_version,
                                                 long task_id,
                                                 const uint8_t hash[4]) {
  if (search_suggest_service_)
    search_suggest_service_->SearchSuggestionSelected(task_version, task_id,
                                                      hash);
}

void SearchTabHelper::OnOptOutOfSearchSuggestions() {
  if (search_suggest_service_)
    search_suggest_service_->OptOutOfSearchSuggestions();
}

void SearchTabHelper::OnApplyDefaultTheme() {
  if (chrome_colors_service_)
    chrome_colors_service_->ApplyDefaultTheme(web_contents_);
}

void SearchTabHelper::OnApplyAutogeneratedTheme(SkColor color) {
  if (chrome_colors_service_)
    chrome_colors_service_->ApplyAutogeneratedTheme(color, web_contents_);
}

void SearchTabHelper::OnRevertThemeChanges() {
  if (chrome_colors_service_)
    chrome_colors_service_->RevertThemeChanges();
}

void SearchTabHelper::OnConfirmThemeChanges() {
  if (chrome_colors_service_)
    chrome_colors_service_->ConfirmThemeChanges();
}

OmniboxView* SearchTabHelper::GetOmniboxView() {
  return const_cast<OmniboxView*>(
      const_cast<const SearchTabHelper*>(this)->GetOmniboxView());
}

Profile* SearchTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

bool SearchTabHelper::IsInputInProgress() const {
  const OmniboxView* omnibox_view = GetOmniboxView();
  return omnibox_view && omnibox_view->model()->user_input_in_progress() &&
         omnibox_view->model()->focus_state() == OMNIBOX_FOCUS_VISIBLE;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchTabHelper)
