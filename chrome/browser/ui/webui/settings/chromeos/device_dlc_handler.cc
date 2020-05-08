// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_dlc_handler.h"
#include "base/bind.h"
#include "base/values.h"
#include "ui/base/text/bytes_formatting.h"

namespace chromeos {
namespace settings {
namespace {

base::ListValue DlcsWithContentToListValue(
    const dlcservice::DlcsWithContent& dlcs_with_content) {
  base::ListValue dlc_metadata_list;
  for (int i = 0; i < dlcs_with_content.dlc_infos_size(); i++) {
    const auto& dlc_info = dlcs_with_content.dlc_infos(i);
    base::Value dlc_metadata(base::Value::Type::DICTIONARY);
    dlc_metadata.SetKey("id", base::Value(dlc_info.id()));
    dlc_metadata.SetKey("name", base::Value(dlc_info.name()));
    dlc_metadata.SetKey("description", base::Value(dlc_info.description()));
    dlc_metadata.SetKey(
        "diskUsageLabel",
        base::Value(ui::FormatBytes(dlc_info.used_bytes_on_disk())));
    dlc_metadata_list.Append(std::move(dlc_metadata));
  }
  return dlc_metadata_list;
}

}  // namespace

DlcHandler::DlcHandler() = default;

DlcHandler::~DlcHandler() = default;

void DlcHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDlcList", base::BindRepeating(&DlcHandler::HandleGetDlcList,
                                        weak_ptr_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "purgeDlc", base::BindRepeating(&DlcHandler::HandlePurgeDlc,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void DlcHandler::OnJavascriptDisallowed() {
  // Ensure that pending callbacks do not complete and cause JS to be evaluated.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void DlcHandler::HandleGetDlcList(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  DlcserviceClient::Get()->GetExistingDlcs(
      base::BindOnce(&DlcHandler::GetDlcListCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback_id->Clone()));
}

void DlcHandler::HandlePurgeDlc(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  std::string dlcId;
  CHECK(args->GetString(1, &dlcId));

  DlcserviceClient::Get()->Purge(
      dlcId,
      base::BindOnce(&DlcHandler::PurgeDlcCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback_id->Clone()));
}

void DlcHandler::GetDlcListCallback(
    const base::Value& callback_id,
    const std::string& err,
    const dlcservice::DlcsWithContent& dlcs_with_content) {
  if (err == dlcservice::kErrorNone) {
    ResolveJavascriptCallback(callback_id,
                              DlcsWithContentToListValue(dlcs_with_content));
    return;
  }
  ResolveJavascriptCallback(callback_id, base::ListValue());
}

void DlcHandler::PurgeDlcCallback(const base::Value& callback_id,
                                  const std::string& err) {
  ResolveJavascriptCallback(callback_id,
                            base::Value(err == dlcservice::kErrorNone));
}

}  // namespace settings
}  // namespace chromeos
