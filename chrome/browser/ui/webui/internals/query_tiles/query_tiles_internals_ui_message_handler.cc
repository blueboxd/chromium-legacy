// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/query_tiles/query_tiles_internals_ui_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/tile_service_factory.h"
#include "components/query_tiles/tile_service.h"
#include "content/public/browser/web_ui.h"

QueryTilesInternalsUIMessageHandler::QueryTilesInternalsUIMessageHandler(
    Profile* profile)
    : tile_service_(query_tiles::TileServiceFactory::GetForKey(
          profile->GetProfileKey())) {
  DCHECK(tile_service_);
}

QueryTilesInternalsUIMessageHandler::~QueryTilesInternalsUIMessageHandler() =
    default;

void QueryTilesInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startFetch", base::BindRepeating(
                        &QueryTilesInternalsUIMessageHandler::HandleStartFetch,
                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "purgeDb",
      base::BindRepeating(&QueryTilesInternalsUIMessageHandler::HandlePurgeDb,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getServiceStatus",
      base::Bind(&QueryTilesInternalsUIMessageHandler::HandleGetServiceStatus,
                 weak_ptr_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "getTileData",
      base::Bind(&QueryTilesInternalsUIMessageHandler::HandleGetTileData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void QueryTilesInternalsUIMessageHandler::HandleGetTileData(
    const base::ListValue* args) {
  NOTIMPLEMENTED();
}

void QueryTilesInternalsUIMessageHandler::HandleGetServiceStatus(
    const base::ListValue* args) {
  NOTIMPLEMENTED();
}

void QueryTilesInternalsUIMessageHandler::HandleStartFetch(
    const base::ListValue* args) {
  AllowJavascript();
  tile_service_->StartFetchForTiles(false /*is_from_reduce_mode*/,
                                    base::BindOnce([](bool reschedule) {}));
}

void QueryTilesInternalsUIMessageHandler::HandlePurgeDb(
    const base::ListValue* args) {
  tile_service_->PurgeDb();
}
