// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/sysmem_buffer_pool.h"

#include <zircon/rights.h>
#include <algorithm>

#include "base/bind.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "media/fuchsia/common/sysmem_buffer_reader.h"
#include "media/fuchsia/common/sysmem_buffer_writer.h"

namespace media {
namespace {

template <typename T>
using CreateAccessorCB = base::OnceCallback<void(std::unique_ptr<T>)>;

template <typename T>
void CreateAccessorInstance(
    zx_status_t status,
    fuchsia::sysmem::BufferCollectionInfo_2 buffer_collection_info,
    CreateAccessorCB<T> create_cb) {
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "Fail to enable reader or writer";
    std::move(create_cb).Run(nullptr);
    return;
  }

  std::unique_ptr<T> accessor = T::Create(std::move(buffer_collection_info));
  if (!accessor) {
    std::move(create_cb).Run(nullptr);
    return;
  }

  std::move(create_cb).Run(std::move(accessor));
}

template <typename T>
void CreateAccessor(fuchsia::sysmem::BufferCollection* collection,
                    CreateAccessorCB<T> create_cb) {
  collection->WaitForBuffersAllocated(
      [create_cb = std::move(create_cb)](zx_status_t status,
                                         fuchsia::sysmem::BufferCollectionInfo_2
                                             buffer_collection_info) mutable {
        CreateAccessorInstance<T>(status, std::move(buffer_collection_info),
                                  std::move(create_cb));
      });
}

}  // namespace

SysmemBufferPool::Creator::Creator(
    fuchsia::sysmem::BufferCollectionPtr collection,
    std::vector<fuchsia::sysmem::BufferCollectionTokenPtr> shared_tokens)
    : collection_(std::move(collection)),
      shared_tokens_(std::move(shared_tokens)) {
  collection_.set_error_handler(
      [this](zx_status_t status) {
        ZX_DLOG(ERROR, status)
            << "Connection to BufferCollection was disconnected.";
        collection_.Unbind();

        if (create_cb_)
          std::move(create_cb_).Run(nullptr);
      });
}

SysmemBufferPool::Creator::~Creator() = default;

void SysmemBufferPool::Creator::Create(
    fuchsia::sysmem::BufferCollectionConstraints constraints,
    CreateCB create_cb) {
  DCHECK(!create_cb_);
  create_cb_ = std::move(create_cb);
  // BufferCollection needs to be synchronized to ensure that all token
  // duplicate requests have been processed and sysmem knows about all clients
  // that will be using this buffer collection.
  collection_->Sync([this, constraints = std::move(constraints)]() mutable {
    collection_->SetConstraints(true /* has constraints */,
                                std::move(constraints));

    DCHECK(create_cb_);
    std::move(create_cb_)
        .Run(std::make_unique<SysmemBufferPool>(std::move(collection_),
                                                std::move(shared_tokens_)));
  });
}

SysmemBufferPool::SysmemBufferPool(
    fuchsia::sysmem::BufferCollectionPtr collection,
    std::vector<fuchsia::sysmem::BufferCollectionTokenPtr> shared_tokens)
    : collection_(std::move(collection)),
      shared_tokens_(std::move(shared_tokens)) {
  collection_.set_error_handler(
      [this](zx_status_t status) {
        ZX_LOG(ERROR, status)
            << "The fuchsia.sysmem.BufferCollection channel was disconnected.";
        collection_.Unbind();
        if (create_reader_cb_)
          std::move(create_reader_cb_).Run(nullptr);
        if (create_writer_cb_)
          std::move(create_writer_cb_).Run(nullptr);
      });
}

SysmemBufferPool::~SysmemBufferPool() = default;

fuchsia::sysmem::BufferCollectionTokenPtr SysmemBufferPool::TakeToken() {
  DCHECK(!shared_tokens_.empty());
  auto token = std::move(shared_tokens_.back());
  shared_tokens_.pop_back();
  return token;
}

void SysmemBufferPool::CreateReader(CreateReaderCB create_cb) {
  DCHECK(!create_reader_cb_);
  create_reader_cb_ = std::move(create_cb);

  CreateAccessor<SysmemBufferReader>(
      collection_.get(), base::BindOnce(&SysmemBufferPool::OnReaderCreated,
                                        base::Unretained(this)));
}

void SysmemBufferPool::OnReaderCreated(
    std::unique_ptr<SysmemBufferReader> reader) {
  DCHECK(create_reader_cb_);
  std::move(create_reader_cb_).Run(std::move(reader));
}

void SysmemBufferPool::CreateWriter(CreateWriterCB create_cb) {
  DCHECK(!create_writer_cb_);
  create_writer_cb_ = std::move(create_cb);

  CreateAccessor<SysmemBufferWriter>(
      collection_.get(), base::BindOnce(&SysmemBufferPool::OnWriterCreated,
                                        base::Unretained(this)));
}

void SysmemBufferPool::OnWriterCreated(
    std::unique_ptr<SysmemBufferWriter> writer) {
  DCHECK(create_writer_cb_);
  std::move(create_writer_cb_).Run(std::move(writer));
}

BufferAllocator::BufferAllocator() {
  allocator_ = base::fuchsia::ComponentContextForCurrentProcess()
                   ->svc()
                   ->Connect<fuchsia::sysmem::Allocator>();

  allocator_.set_error_handler([](zx_status_t status) {
    // Just log a warning. We will handle BufferCollection the failure when
    // trying to create a new BufferCollection.
    ZX_DLOG(WARNING, status)
        << "The fuchsia.sysmem.Allocator channel was disconnected.";
  });
}

BufferAllocator::~BufferAllocator() = default;

std::unique_ptr<SysmemBufferPool::Creator>
BufferAllocator::MakeBufferPoolCreator(size_t num_of_tokens) {
  // Create a new sysmem buffer collection token for the allocated buffers.
  fuchsia::sysmem::BufferCollectionTokenPtr collection_token;
  allocator_->AllocateSharedCollection(collection_token.NewRequest());

  // Create collection token for sharing with other components.
  std::vector<fuchsia::sysmem::BufferCollectionTokenPtr> shared_tokens;
  for (size_t i = 0; i < num_of_tokens; i++) {
    fuchsia::sysmem::BufferCollectionTokenPtr token;
    collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS, token.NewRequest());
    shared_tokens.push_back(std::move(token));
  }

  fuchsia::sysmem::BufferCollectionPtr buffer_collection;

  // Convert the token to a BufferCollection connection.
  allocator_->BindSharedCollection(std::move(collection_token),
                                   buffer_collection.NewRequest());

  return std::make_unique<SysmemBufferPool::Creator>(
      std::move(buffer_collection), std::move(shared_tokens));
}

}  // namespace media
