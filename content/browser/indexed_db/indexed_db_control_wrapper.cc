// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_control_wrapper.h"

namespace content {

IndexedDBControlWrapper::IndexedDBControlWrapper(
    scoped_refptr<IndexedDBContextImpl> context)
    : context_(std::move(context)) {}

IndexedDBControlWrapper::~IndexedDBControlWrapper() = default;

void IndexedDBControlWrapper::BindIndexedDB(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  // TODO(enne): track special storage policy here.
  BindRemoteIfNeeded();
  indexed_db_control_->BindIndexedDB(origin, std::move(receiver));
}

void IndexedDBControlWrapper::GetUsage(GetUsageCallback usage_callback) {
  BindRemoteIfNeeded();
  indexed_db_control_->GetUsage(std::move(usage_callback));
}

void IndexedDBControlWrapper::DeleteForOrigin(
    const url::Origin& origin,
    DeleteForOriginCallback callback) {
  BindRemoteIfNeeded();
  indexed_db_control_->DeleteForOrigin(origin, std::move(callback));
}

void IndexedDBControlWrapper::ForceClose(
    const url::Origin& origin,
    storage::mojom::ForceCloseReason reason,
    base::OnceClosure callback) {
  BindRemoteIfNeeded();
  indexed_db_control_->ForceClose(origin, reason, std::move(callback));
}

void IndexedDBControlWrapper::GetConnectionCount(
    const url::Origin& origin,
    GetConnectionCountCallback callback) {
  BindRemoteIfNeeded();
  indexed_db_control_->GetConnectionCount(origin, std::move(callback));
}

void IndexedDBControlWrapper::DownloadOriginData(
    const url::Origin& origin,
    DownloadOriginDataCallback callback) {
  BindRemoteIfNeeded();
  indexed_db_control_->DownloadOriginData(origin, std::move(callback));
}

void IndexedDBControlWrapper::GetAllOriginsDetails(
    GetAllOriginsDetailsCallback callback) {
  BindRemoteIfNeeded();
  indexed_db_control_->GetAllOriginsDetails(std::move(callback));
}

void IndexedDBControlWrapper::SetForceKeepSessionState() {
  BindRemoteIfNeeded();
  indexed_db_control_->SetForceKeepSessionState();
}

void IndexedDBControlWrapper::BindTestInterface(
    mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver) {
  BindRemoteIfNeeded();
  indexed_db_control_->BindTestInterface(std::move(receiver));
}

void IndexedDBControlWrapper::AddObserver(
    mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) {
  BindRemoteIfNeeded();
  indexed_db_control_->AddObserver(std::move(observer));
}

void IndexedDBControlWrapper::BindRemoteIfNeeded() {
  DCHECK(
      !(indexed_db_control_.is_bound() && !indexed_db_control_.is_connected()))
      << "Rebinding is not supported yet.";

  if (indexed_db_control_.is_bound())
    return;
  IndexedDBContextImpl* idb_context = GetIndexedDBContextInternal();
  idb_context->IDBTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IndexedDBContextImpl::Bind,
                     base::WrapRefCounted(idb_context),
                     indexed_db_control_.BindNewPipeAndPassReceiver()));
}

}  // namespace content
