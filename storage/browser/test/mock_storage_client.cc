// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_storage_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "url/gurl.h"

namespace storage {

MockStorageClient::MockStorageClient(
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    const MockOriginData* mock_data,
    QuotaClient::ID id,
    size_t mock_data_size)
    : quota_manager_proxy_(std::move(quota_manager_proxy)),
      id_(id),
      mock_time_counter_(0) {
  Populate(mock_data, mock_data_size);
}

void MockStorageClient::Populate(
    const MockOriginData* mock_data,
    size_t mock_data_size) {
  for (size_t i = 0; i < mock_data_size; ++i) {
    // TODO(crbug.com/889590): Use helper for url::Origin creation from string.
    origin_data_[std::make_pair(url::Origin::Create(GURL(mock_data[i].origin)),
                                mock_data[i].type)] = mock_data[i].usage;
  }
}

MockStorageClient::~MockStorageClient() = default;

void MockStorageClient::AddOriginAndNotify(const url::Origin& origin,
                                           blink::mojom::StorageType type,
                                           int64_t size) {
  DCHECK(origin_data_.count(std::make_pair(origin, type)) == 0);
  DCHECK_GE(size, 0);
  origin_data_[std::make_pair(origin, type)] = size;
  quota_manager_proxy_->quota_manager()->NotifyStorageModifiedInternal(
      id(), origin, type, size, IncrementMockTime());
}

void MockStorageClient::ModifyOriginAndNotify(const url::Origin& origin,
                                              blink::mojom::StorageType type,
                                              int64_t delta) {
  auto it = origin_data_.find(std::make_pair(origin, type));
  DCHECK(it != origin_data_.end());
  it->second += delta;
  DCHECK_GE(it->second, 0);

  // TODO(tzik): Check quota to prevent usage exceed
  quota_manager_proxy_->quota_manager()->NotifyStorageModifiedInternal(
      id(), origin, type, delta, IncrementMockTime());
}

void MockStorageClient::TouchAllOriginsAndNotify() {
  for (const auto& origin_type : origin_data_) {
    quota_manager_proxy_->quota_manager()->NotifyStorageModifiedInternal(
        id(), origin_type.first.first, origin_type.first.second, 0,
        IncrementMockTime());
  }
}

void MockStorageClient::AddOriginToErrorSet(const url::Origin& origin,
                                            blink::mojom::StorageType type) {
  error_origins_.insert(std::make_pair(origin, type));
}

base::Time MockStorageClient::IncrementMockTime() {
  ++mock_time_counter_;
  return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
}

QuotaClient::ID MockStorageClient::id() const {
  return id_;
}

void MockStorageClient::OnQuotaManagerDestroyed() {}

void MockStorageClient::GetOriginUsage(const url::Origin& origin,
                                       blink::mojom::StorageType type,
                                       GetUsageCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockStorageClient::RunGetOriginUsage,
                                weak_factory_.GetWeakPtr(), origin, type,
                                std::move(callback)));
}

void MockStorageClient::GetOriginsForType(blink::mojom::StorageType type,
                                          GetOriginsCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockStorageClient::RunGetOriginsForType,
                     weak_factory_.GetWeakPtr(), type, std::move(callback)));
}

void MockStorageClient::GetOriginsForHost(blink::mojom::StorageType type,
                                          const std::string& host,
                                          GetOriginsCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockStorageClient::RunGetOriginsForHost,
                                weak_factory_.GetWeakPtr(), type, host,
                                std::move(callback)));
}

void MockStorageClient::DeleteOriginData(const url::Origin& origin,
                                         blink::mojom::StorageType type,
                                         DeletionCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockStorageClient::RunDeleteOriginData,
                                weak_factory_.GetWeakPtr(), origin, type,
                                std::move(callback)));
}

void MockStorageClient::PerformStorageCleanup(blink::mojom::StorageType type,
                                              base::OnceClosure callback) {
  std::move(callback).Run();
}

bool MockStorageClient::DoesSupport(blink::mojom::StorageType type) const {
  return true;
}

void MockStorageClient::RunGetOriginUsage(const url::Origin& origin,
                                          blink::mojom::StorageType type,
                                          GetUsageCallback callback) {
  auto it = origin_data_.find(std::make_pair(origin, type));
  if (it == origin_data_.end()) {
    std::move(callback).Run(0);
  } else {
    std::move(callback).Run(it->second);
  }
}

void MockStorageClient::RunGetOriginsForType(blink::mojom::StorageType type,
                                             GetOriginsCallback callback) {
  std::set<url::Origin> origins;
  for (const auto& origin_type_usage : origin_data_) {
    if (type == origin_type_usage.first.second)
      origins.insert(origin_type_usage.first.first);
  }
  std::move(callback).Run(origins);
}

void MockStorageClient::RunGetOriginsForHost(blink::mojom::StorageType type,
                                             const std::string& host,
                                             GetOriginsCallback callback) {
  std::set<url::Origin> origins;
  for (const auto& origin_type_usage : origin_data_) {
    std::string host_or_spec =
        net::GetHostOrSpecFromURL(origin_type_usage.first.first.GetURL());
    if (type == origin_type_usage.first.second && host == host_or_spec)
      origins.insert(origin_type_usage.first.first);
  }
  std::move(callback).Run(origins);
}

void MockStorageClient::RunDeleteOriginData(const url::Origin& origin,
                                            blink::mojom::StorageType type,
                                            DeletionCallback callback) {
  auto error_it = error_origins_.find(std::make_pair(origin, type));
  if (error_it != error_origins_.end()) {
    std::move(callback).Run(
        blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    return;
  }

  auto it = origin_data_.find(std::make_pair(origin, type));
  if (it != origin_data_.end()) {
    int64_t delta = it->second;
    quota_manager_proxy_->NotifyStorageModified(id(), origin, type, -delta);
    origin_data_.erase(it);
  }

  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
}

}  // namespace storage
