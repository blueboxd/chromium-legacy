// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_

#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"

namespace content {

class CONTENT_EXPORT IndexedDBControlWrapper
    : public storage::mojom::IndexedDBControl {
 public:
  explicit IndexedDBControlWrapper(scoped_refptr<IndexedDBContextImpl> context);
  ~IndexedDBControlWrapper() override;

  // mojom::IndexedDBControl implementation:
  void BindIndexedDB(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) override;
  void GetUsage(GetUsageCallback usage_callback) override;
  void DeleteForOrigin(const url::Origin& origin,
                       DeleteForOriginCallback callback) override;
  void ForceClose(const url::Origin& origin,
                  storage::mojom::ForceCloseReason reason,
                  base::OnceClosure callback) override;
  void GetConnectionCount(const url::Origin& origin,
                          GetConnectionCountCallback callback) override;
  void DownloadOriginData(const url::Origin& origin,
                          DownloadOriginDataCallback callback) override;
  void GetAllOriginsDetails(GetAllOriginsDetailsCallback callback) override;
  void SetForceKeepSessionState() override;
  void BindTestInterface(
      mojo::PendingReceiver<storage::mojom::IndexedDBControlTest> receiver)
      override;
  void AddObserver(
      mojo::PendingRemote<storage::mojom::IndexedDBObserver> observer) override;

  // TODO(enne): remove this once IndexedDB moves to storage service.
  IndexedDBContextImpl* GetIndexedDBContextInternal() { return context_.get(); }

 private:
  void BindRemoteIfNeeded();

  mojo::Remote<storage::mojom::IndexedDBControl> indexed_db_control_;
  scoped_refptr<IndexedDBContextImpl> context_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBControlWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTROL_WRAPPER_H_
