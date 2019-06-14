// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/cookie_info_view.h"

#include <algorithm>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_delegate.h"

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, public:

CookieInfoView::CookieInfoView() = default;

CookieInfoView::~CookieInfoView() = default;

void CookieInfoView::SetCookie(const std::string& domain,
                               const net::CanonicalCookie& cookie) {
  name_value_field_->SetText(base::UTF8ToUTF16(cookie.Name()));
  content_value_field_->SetText(base::UTF8ToUTF16(cookie.Value()));
  domain_value_field_->SetText(base::UTF8ToUTF16(domain));
  path_value_field_->SetText(base::UTF8ToUTF16(cookie.Path()));
  created_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(cookie.CreationDate()));

  base::string16 expire_text = cookie.IsPersistent() ?
      base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate()) :
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_EXPIRES_SESSION);

  expires_value_field_->SetText(expire_text);
  send_for_value_field_->SetText(
      l10n_util::GetStringUTF16(CookiesTreeModel::GetSendForMessageID(cookie)));
  EnableCookieDisplay(true);
  Layout();
}

void CookieInfoView::ClearCookieDisplay() {
  base::string16 no_cookie_string =
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_NONESELECTED);
  name_value_field_->SetText(no_cookie_string);
  content_value_field_->SetText(no_cookie_string);
  domain_value_field_->SetText(no_cookie_string);
  path_value_field_->SetText(no_cookie_string);
  send_for_value_field_->SetText(no_cookie_string);
  created_value_field_->SetText(no_cookie_string);
  expires_value_field_->SetText(no_cookie_string);
  EnableCookieDisplay(false);
}

void CookieInfoView::EnableCookieDisplay(bool enabled) {
  name_value_field_->SetEnabled(enabled);
  content_value_field_->SetEnabled(enabled);
  domain_value_field_->SetEnabled(enabled);
  path_value_field_->SetEnabled(enabled);
  send_for_value_field_->SetEnabled(enabled);
  created_value_field_->SetEnabled(enabled);
  expires_value_field_->SetEnabled(enabled);
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, views::View overrides.

void CookieInfoView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    Init();
}

views::Textfield* CookieInfoView::AddLabelRow(int layout_id,
                                              views::GridLayout* layout,
                                              int label_message_id) {
  auto textfield = std::make_unique<views::Textfield>();
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(label_message_id));
  textfield->SetAssociatedLabel(label.get());
  layout->StartRow(views::GridLayout::kFixedSize, layout_id);
  layout->AddView(std::move(label));
  auto* textfield_ptr =
      layout->AddView(std::move(textfield), 2, 1, views::GridLayout::FILL,
                      views::GridLayout::CENTER);

  // Now that the Textfield is in the view hierarchy, it can be initialized.
  textfield_ptr->SetReadOnly(true);
  textfield_ptr->SetBorder(views::NullBorder());
  // Color these borderless text areas the same as the containing dialog.
  textfield_ptr->SetBackgroundColor(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground));
  textfield_ptr->SetTextColor(SkColorSetRGB(0x78, 0x78, 0x78));
  return textfield_ptr;
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, private:

void CookieInfoView::Init() {
  constexpr int kLabelValuePadding = 96;

  // Ensure we don't run this more than once and leak memory.
  DCHECK(!name_value_field_);

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets& dialog_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  SetBorder(views::CreateEmptyBorder(0, dialog_insets.left(), 0,
                                     dialog_insets.right()));

  int three_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(
      provider->GetControlLabelGridAlignment(), views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               kLabelValuePadding);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                        views::GridLayout::USE_PREF, 0, 0);

  name_value_field_ = AddLabelRow(three_column_layout_id, layout,
                                  IDS_COOKIES_COOKIE_NAME_LABEL);

  content_value_field_ = AddLabelRow(three_column_layout_id, layout,
                                     IDS_COOKIES_COOKIE_CONTENT_LABEL);

  domain_value_field_ = AddLabelRow(three_column_layout_id, layout,
                                    IDS_COOKIES_COOKIE_DOMAIN_LABEL);

  path_value_field_ = AddLabelRow(three_column_layout_id, layout,
                                  IDS_COOKIES_COOKIE_PATH_LABEL);

  send_for_value_field_ = AddLabelRow(three_column_layout_id, layout,
                                      IDS_COOKIES_COOKIE_SENDFOR_LABEL);

  created_value_field_ = AddLabelRow(three_column_layout_id, layout,
                                     IDS_COOKIES_COOKIE_CREATED_LABEL);

  expires_value_field_ = AddLabelRow(three_column_layout_id, layout,
                                     IDS_COOKIES_COOKIE_EXPIRES_LABEL);
}
