// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/sqlite_trust_token_persister.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "components/sqlite_proto/key_value_data.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace network {

namespace {
std::string ToKey(const url::Origin& issuer, const url::Origin& toplevel) {
  DCHECK(issuer.GetURL().SchemeIsHTTPOrHTTPS());
  DCHECK(toplevel.GetURL().SchemeIsHTTPOrHTTPS());

  // U+0020 space is a character forbidden in schemes/hosts/ports, so it
  // shouldn't appear in the serialization of either origin, preventing
  // collisions.
  return issuer.Serialize() + " " + toplevel.Serialize();
}

void OnDatabaseOwnerCreated(
    base::OnceCallback<void(std::unique_ptr<SQLiteTrustTokenPersister>)>
        on_done_initializing,
    std::unique_ptr<TrustTokenDatabaseOwner> database_owner) {
  auto ret =
      std::make_unique<SQLiteTrustTokenPersister>(std::move(database_owner));
  std::move(on_done_initializing).Run(std::move(ret));
}

}  // namespace

SQLiteTrustTokenPersister::SQLiteTrustTokenPersister(
    std::unique_ptr<TrustTokenDatabaseOwner> database_owner)
    : database_owner_(std::move(database_owner)) {}

SQLiteTrustTokenPersister::~SQLiteTrustTokenPersister() = default;

void SQLiteTrustTokenPersister::CreateForFilePath(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const base::FilePath& path,
    base::TimeDelta flush_delay_for_writes,
    base::OnceCallback<void(std::unique_ptr<SQLiteTrustTokenPersister>)>
        on_done_initializing) {
  TrustTokenDatabaseOwner::Create(
      /*db_opener=*/base::BindOnce(
          [](const base::FilePath& path, sql::Database* db) {
            return db->Open(path);
          },
          path),
      db_task_runner, flush_delay_for_writes,
      base::BindOnce(&OnDatabaseOwnerCreated, std::move(on_done_initializing)));
}

std::unique_ptr<TrustTokenIssuerConfig>
SQLiteTrustTokenPersister::GetIssuerConfig(const url::Origin& issuer) {
  DCHECK(issuer.GetURL().SchemeIsHTTPOrHTTPS());

  auto* data = database_owner_->IssuerData();
  CHECK(data);

  auto ret = std::make_unique<TrustTokenIssuerConfig>();
  return data->TryGetData(issuer.Serialize(), ret.get()) ? std::move(ret)
                                                         : nullptr;
}

std::unique_ptr<TrustTokenToplevelConfig>
SQLiteTrustTokenPersister::GetToplevelConfig(const url::Origin& toplevel) {
  DCHECK(toplevel.GetURL().SchemeIsHTTPOrHTTPS());

  auto* data = database_owner_->ToplevelData();
  CHECK(data);

  auto ret = std::make_unique<TrustTokenToplevelConfig>();
  return data->TryGetData(toplevel.Serialize(), ret.get()) ? std::move(ret)
                                                           : nullptr;
}

std::unique_ptr<TrustTokenIssuerToplevelPairConfig>
SQLiteTrustTokenPersister::GetIssuerToplevelPairConfig(
    const url::Origin& issuer,
    const url::Origin& toplevel) {
  DCHECK(issuer.GetURL().SchemeIsHTTPOrHTTPS());
  DCHECK(toplevel.GetURL().SchemeIsHTTPOrHTTPS());

  auto* data = database_owner_->IssuerToplevelPairData();
  CHECK(data);

  auto ret = std::make_unique<TrustTokenIssuerToplevelPairConfig>();
  return data->TryGetData(ToKey(issuer, toplevel), ret.get()) ? std::move(ret)
                                                              : nullptr;
}

void SQLiteTrustTokenPersister::SetIssuerConfig(
    const url::Origin& issuer,
    std::unique_ptr<TrustTokenIssuerConfig> config) {
  DCHECK(issuer.GetURL().SchemeIsHTTPOrHTTPS());

  sqlite_proto::KeyValueData<TrustTokenIssuerConfig>* data =
      database_owner_->IssuerData();
  CHECK(data);
  data->UpdateData(issuer.Serialize(), *config);
}

void SQLiteTrustTokenPersister::SetToplevelConfig(
    const url::Origin& toplevel,
    std::unique_ptr<TrustTokenToplevelConfig> config) {
  DCHECK(toplevel.GetURL().SchemeIsHTTPOrHTTPS());

  sqlite_proto::KeyValueData<TrustTokenToplevelConfig>* data =
      database_owner_->ToplevelData();
  CHECK(data);
  data->UpdateData(toplevel.Serialize(), *config);
}

void SQLiteTrustTokenPersister::SetIssuerToplevelPairConfig(
    const url::Origin& issuer,
    const url::Origin& toplevel,
    std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config) {
  DCHECK(issuer.GetURL().SchemeIsHTTPOrHTTPS());
  DCHECK(toplevel.GetURL().SchemeIsHTTPOrHTTPS());

  sqlite_proto::KeyValueData<TrustTokenIssuerToplevelPairConfig>* data =
      database_owner_->IssuerToplevelPairData();
  CHECK(data);
  data->UpdateData(ToKey(issuer, toplevel), *config);
}

}  // namespace network
