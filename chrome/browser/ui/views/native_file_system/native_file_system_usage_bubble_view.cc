// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_usage_bubble_view.h"

#include "base/i18n/message_formatter.h"
#include "base/i18n/unicodestring.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_ui_helpers.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/listformatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Wrapper around icu::ListFormatter to format a list of strings.
base::string16 FormatList(base::span<base::string16> items) {
  std::vector<icu::UnicodeString> strings;
  strings.reserve(items.size());
  for (const auto& item : items)
    strings.emplace_back(item.data(), item.size());
  UErrorCode error = U_ZERO_ERROR;
  auto formatter = base::WrapUnique(icu::ListFormatter::createInstance(error));
  if (U_FAILURE(error) || !formatter) {
    LOG(ERROR) << "ListFormatter failed with " << u_errorName(error);
    return base::string16();
  }
  icu::UnicodeString formatted;
  formatter->format(strings.data(), strings.size(), formatted, error);
  if (U_FAILURE(error)) {
    LOG(ERROR) << "ListFormatter failed with " << u_errorName(error);
    return base::string16();
  }
  return base::i18n::UnicodeStringToString16(formatted);
}

// Returns the message Id to use as heading text, depending on what types of
// usage are present (i.e. just writable files, or also readable directories,
// etc).
// |need_lifetime_text_at_end| is set to false iff the returned message Id
// already includes an explanation for how long a website will have access to
// the listed paths. It is set to true iff a separate label is needed at the end
// of the dialog to explain lifetime.
int ComputeHeadingMessageFromUsage(
    const NativeFileSystemUsageBubbleView::Usage& usage,
    bool* need_lifetime_text_at_end) {
  if (!usage.writable_files.empty() && !usage.writable_directories.empty()) {
    *need_lifetime_text_at_end = true;
    return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_WRITABLE_FILES_AND_DIRECTORIES_TEXT;
  }
  if (!usage.writable_files.empty()) {
    if (usage.readable_directories.empty()) {
      *need_lifetime_text_at_end = false;
      return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_WRITABLE_FILES_TEXT;
    }
    *need_lifetime_text_at_end = true;
    return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_WRITABLE_FILES_NO_LIFETIME_TEXT;
  }
  if (!usage.writable_directories.empty()) {
    if (usage.readable_directories.empty()) {
      *need_lifetime_text_at_end = false;
      return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_WRITABLE_DIRECTORIES_TEXT;
    }
    *need_lifetime_text_at_end = true;
    return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_WRITABLE_DIRECTORIES_NO_LIFETIME_TEXT;
  }

  DCHECK(!usage.readable_directories.empty());
  *need_lifetime_text_at_end = false;
  return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_READABLE_DIRECTORIES_TEXT;
}

}  // namespace

NativeFileSystemUsageBubbleView::Usage::Usage() = default;
NativeFileSystemUsageBubbleView::Usage::~Usage() = default;
NativeFileSystemUsageBubbleView::Usage::Usage(Usage&&) = default;
NativeFileSystemUsageBubbleView::Usage& NativeFileSystemUsageBubbleView::Usage::
operator=(Usage&&) = default;

// static
NativeFileSystemUsageBubbleView* NativeFileSystemUsageBubbleView::bubble_ =
    nullptr;

// static
void NativeFileSystemUsageBubbleView::ShowBubble(
    content::WebContents* web_contents,
    const url::Origin& origin,
    Usage usage) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  OmniboxPageActionIconContainerView* anchor_view =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetOmniboxPageActionIconContainerView();

  bubble_ = new NativeFileSystemUsageBubbleView(
      anchor_view, gfx::Point(), web_contents, origin, std::move(usage));

  bubble_->SetHighlightedButton(anchor_view->GetPageActionIconView(
      PageActionIconType::kNativeFileSystemAccess));
  views::BubbleDialogDelegateView::CreateBubble(bubble_);

  bubble_->ShowForReason(DisplayReason::USER_GESTURE,
                         /*allow_refocus_alert=*/true);
}

// static
void NativeFileSystemUsageBubbleView::CloseCurrentBubble() {
  if (bubble_)
    bubble_->CloseBubble();
}

// static
NativeFileSystemUsageBubbleView* NativeFileSystemUsageBubbleView::GetBubble() {
  return bubble_;
}

NativeFileSystemUsageBubbleView::NativeFileSystemUsageBubbleView(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    content::WebContents* web_contents,
    const url::Origin& origin,
    Usage usage)
    : LocationBarBubbleDelegateView(anchor_view, anchor_point, web_contents),
      origin_(origin),
      usage_(std::move(usage)) {}

NativeFileSystemUsageBubbleView::~NativeFileSystemUsageBubbleView() = default;

base::string16 NativeFileSystemUsageBubbleView::GetAccessibleWindowTitle()
    const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  // Don't crash if the web_contents is destroyed/unloaded.
  if (!browser)
    return {};

  OmniboxPageActionIconContainerView* page_action_icon_container_view =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetOmniboxPageActionIconContainerView();
  if (!page_action_icon_container_view)
    return {};

  PageActionIconView* icon_view =
      page_action_icon_container_view->GetPageActionIconView(
          PageActionIconType::kNativeFileSystemAccess);
  return icon_view->GetTextForTooltipAndAccessibleName();
}

int NativeFileSystemUsageBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 NativeFileSystemUsageBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_DONE);
}

bool NativeFileSystemUsageBubbleView::ShouldShowCloseButton() const {
  return false;
}

void NativeFileSystemUsageBubbleView::Init() {
  // Set up the layout of the bubble.
  const views::LayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));

  bool need_lifetime_text_at_end = false;
  int heading_message_id =
      ComputeHeadingMessageFromUsage(usage_, &need_lifetime_text_at_end);

  AddChildView(native_file_system_ui_helper::CreateOriginLabel(
      heading_message_id, origin_, CONTEXT_BODY_TEXT_LARGE));

  AddPathList(IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_FILES_TEXT,
              usage_.writable_files);
  AddPathList(IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_DIRECTORIES_TEXT,
              usage_.writable_directories);

  // If the header wasn't already the "readable directories" header (i.e. we
  // had at least one writable file or directory as well) add a secondary header
  // for the readable directories section.
  if (heading_message_id !=
      IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_READABLE_DIRECTORIES_TEXT) {
    auto directory_label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(
            IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_ALSO_READABLE_DIRECTORIES_TEXT),
        CONTEXT_BODY_TEXT_LARGE, STYLE_SECONDARY);
    directory_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    directory_label->SetMultiLine(true);
    AddChildView(std::move(directory_label));
  }

  AddPathList(IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_DIRECTORIES_TEXT,
              usage_.readable_directories);

  if (need_lifetime_text_at_end) {
    auto lifetime_label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(
            IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_LIFETIME_TEXT),
        CONTEXT_BODY_TEXT_LARGE, STYLE_SECONDARY);
    lifetime_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    lifetime_label->SetMultiLine(true);
    AddChildView(std::move(lifetime_label));
  }
}

void NativeFileSystemUsageBubbleView::WindowClosing() {
  // |bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to nullptr when it's this bubble.
  if (bubble_ == this)
    bubble_ = nullptr;
}

void NativeFileSystemUsageBubbleView::CloseBubble() {
  // Widget's Close() is async, but we don't want to use bubble_ after
  // this. Additionally web_contents() may have been destroyed.
  bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

gfx::Size NativeFileSystemUsageBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void NativeFileSystemUsageBubbleView::AddPathList(
    int details_message_id,
    const std::vector<base::FilePath>& paths) {
  if (paths.empty())
    return;

  std::vector<base::string16> base_names;
  for (const auto& path : paths)
    base_names.push_back(path.BaseName().LossyDisplayName());
  base::string16 path_list = FormatList(base_names);
  auto paths_label = std::make_unique<views::Label>(
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(details_message_id), int64_t{paths.size()},
          path_list),
      CONTEXT_BODY_TEXT_SMALL, STYLE_EMPHASIZED_SECONDARY);
  paths_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(paths_label));
}
