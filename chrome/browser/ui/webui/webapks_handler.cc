// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webapks_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/android/color_helpers.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "content/public/browser/web_ui.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/gfx/color_utils.h"

WebApksHandler::WebApksHandler() : weak_ptr_factory_(this) {}

WebApksHandler::~WebApksHandler() {}

void WebApksHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestWebApksInfo",
      base::BindRepeating(&WebApksHandler::HandleRequestWebApksInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestWebApkUpdate",
      base::BindRepeating(&WebApksHandler::HandleRequestWebApkUpdate,
                          base::Unretained(this)));
}

void WebApksHandler::HandleRequestWebApksInfo(const base::ListValue* args) {
  AllowJavascript();
  ShortcutHelper::RetrieveWebApks(base::Bind(
      &WebApksHandler::OnWebApkInfoRetrieved, weak_ptr_factory_.GetWeakPtr()));
}

void WebApksHandler::HandleRequestWebApkUpdate(const base::ListValue* args) {
  AllowJavascript();
  for (const auto& val : args->GetList()) {
    if (val.is_string())
      ShortcutHelper::SetForceWebApkUpdate(val.GetString());
  }
}

void WebApksHandler::OnWebApkInfoRetrieved(
    const std::vector<WebApkInfo>& webapks_list) {
  if (!IsJavascriptAllowed())
    return;
  base::ListValue list;
  for (const auto& webapk_info : webapks_list) {
    auto result = std::make_unique<base::DictionaryValue>();
    result->SetString("name", webapk_info.name);
    result->SetString("shortName", webapk_info.short_name);
    result->SetString("packageName", webapk_info.package_name);
    result->SetString("id", webapk_info.id);
    result->SetInteger("shellApkVersion", webapk_info.shell_apk_version);
    result->SetInteger("versionCode", webapk_info.version_code);
    result->SetString("uri", webapk_info.uri);
    result->SetString("scope", webapk_info.scope);
    result->SetString("manifestUrl", webapk_info.manifest_url);
    result->SetString("manifestStartUrl", webapk_info.manifest_start_url);
    result->SetString("displayMode",
                      blink::WebDisplayModeToString(webapk_info.display));
    result->SetString(
        "orientation",
        blink::WebScreenOrientationLockTypeToString(webapk_info.orientation));
    result->SetString("themeColor",
                      OptionalSkColorToString(webapk_info.theme_color));
    result->SetString("backgroundColor",
                      OptionalSkColorToString(webapk_info.background_color));
    result->SetDouble("lastUpdateCheckTimeMs",
                      webapk_info.last_update_check_time.ToJsTime());
    result->SetBoolean("relaxUpdates", webapk_info.relax_updates);
    result->SetString("updateStatus", webapk_info.update_status);
    list.Append(std::move(result));
  }

  CallJavascriptFunction("returnWebApksInfo", list);
}
