// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/shell_content_client.h"

#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/app/resources/grit/content_resources.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/grit/shell_resources.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace content {

ShellContentClient::ShellContentClient() {}

ShellContentClient::~ShellContentClient() {}

base::string16 ShellContentClient::GetLocalizedString(int message_id) {
  if (switches::IsRunWebTestsSwitchPresent()) {
    switch (message_id) {
      case IDS_FORM_OTHER_DATE_LABEL:
        return base::ASCIIToUTF16("<<OtherDateLabel>>");
      case IDS_FORM_OTHER_MONTH_LABEL:
        return base::ASCIIToUTF16("<<OtherMonthLabel>>");
      case IDS_FORM_OTHER_WEEK_LABEL:
        return base::ASCIIToUTF16("<<OtherWeekLabel>>");
      case IDS_FORM_CALENDAR_CLEAR:
        return base::ASCIIToUTF16("<<CalendarClear>>");
      case IDS_FORM_CALENDAR_TODAY:
        return base::ASCIIToUTF16("<<CalendarToday>>");
      case IDS_FORM_THIS_MONTH_LABEL:
        return base::ASCIIToUTF16("<<ThisMonthLabel>>");
      case IDS_FORM_THIS_WEEK_LABEL:
        return base::ASCIIToUTF16("<<ThisWeekLabel>>");
    }
  }
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ShellContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  if (switches::IsRunWebTestsSwitchPresent()) {
    switch (resource_id) {
      case IDR_BROKENIMAGE:
#if defined(OS_MACOSX)
        resource_id = IDR_CONTENT_SHELL_MISSING_IMAGE_PNG;
#else
        resource_id = IDR_CONTENT_SHELL_MISSING_IMAGE_GIF;
#endif
        break;
    }
  }
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ShellContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

bool ShellContentClient::IsDataResourceGzipped(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().IsGzipped(resource_id);
}

gfx::Image& ShellContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

base::DictionaryValue ShellContentClient::GetNetLogConstants() {
  base::DictionaryValue client_constants;
  client_constants.SetString("name", "content_shell");
  client_constants.SetString(
      "command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
  base::DictionaryValue constants;
  constants.SetKey("clientInfo", std::move(client_constants));
  return constants;
}

blink::OriginTrialPolicy* ShellContentClient::GetOriginTrialPolicy() {
  return &origin_trial_policy_;
}

}  // namespace content
