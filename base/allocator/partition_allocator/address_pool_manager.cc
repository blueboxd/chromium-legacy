// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_pool_manager.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/bits.h"
#include "base/stl_util.h"

#include <limits>

namespace base {
namespace internal {

#if defined(ARCH_CPU_64_BITS)

constexpr size_t AddressPoolManager::kNumPools;

// static
AddressPoolManager* AddressPoolManager::GetInstance() {
  static NoDestructor<AddressPoolManager> instance;
  return instance.get();
}

pool_handle AddressPoolManager::Add(const void* ptr,
                                    size_t length,
                                    size_t align) {
  DCHECK(base::bits::IsPowerOfTwo(align));
  const uintptr_t align_offset_mask = align - 1;
  const uintptr_t ptr_as_uintptr = reinterpret_cast<uintptr_t>(ptr);
  DCHECK(!(ptr_as_uintptr & align_offset_mask));
  DCHECK(!((ptr_as_uintptr + length) & align_offset_mask));

  for (pool_handle i = 0; i < base::size(pools_); ++i) {
    if (!pools_[i]) {
      pools_[i] = std::make_unique<Pool>(ptr_as_uintptr, length, align);
      return i + 1;
    }
  }
  NOTREACHED();
  return 0;
}

void AddressPoolManager::ResetForTesting() {
  for (pool_handle i = 0; i < base::size(pools_); ++i)
    pools_[i].reset();
}

void AddressPoolManager::Remove(pool_handle handle) {
  DCHECK(0 < handle && handle <= kNumPools);
  pools_[handle - 1].reset();
}

void* AddressPoolManager::Alloc(pool_handle handle, size_t length) {
  DCHECK(0 < handle && handle <= kNumPools);
  Pool* pool = pools_[handle - 1].get();
  DCHECK(pool);
  return pool->FindChunk(length);
}

void AddressPoolManager::Free(pool_handle handle, void* ptr, size_t length) {
  DCHECK(0 < handle && handle <= kNumPools);
  Pool* pool = pools_[handle - 1].get();
  DCHECK(pool);
  pool->FreeChunk(reinterpret_cast<uintptr_t>(ptr), length);
}

AddressPoolManager::Pool::Pool(uintptr_t ptr, size_t length, size_t align)
    : align_(align)
#if DCHECK_IS_ON()
      ,
      address_begin_(ptr),
      address_end_(ptr + length)
#endif
{
  free_chunks_.insert(std::make_pair(ptr, length));
#if DCHECK_IS_ON()
  DCHECK_LT(address_begin_, address_end_);
#endif
}

void* AddressPoolManager::Pool::FindChunk(size_t requested_size) {
  base::AutoLock scoped_lock(lock_);

  const uintptr_t align_offset_mask = align_ - 1;
  const size_t required_size = bits::Align(requested_size, align_);
  uintptr_t chosen_chunk = 0;
  size_t chosen_chunk_size = std::numeric_limits<size_t>::max();

  // Use first fit policy to find an available chunk from free chunks.
  for (const auto& chunk : free_chunks_) {
    size_t chunk_size = chunk.second;
    if (chunk_size >= required_size) {
      chosen_chunk = chunk.first;
      chosen_chunk_size = chunk_size;
      break;
    }
  }
  if (!chosen_chunk)
    return nullptr;

  free_chunks_.erase(chosen_chunk);
  if (chosen_chunk_size > required_size) {
    bool newly_inserted =
        free_chunks_
            .insert(std::make_pair(chosen_chunk + required_size,
                                   chosen_chunk_size - required_size))
            .second;
    DCHECK(newly_inserted);
  }
  DCHECK(!(chosen_chunk & align_offset_mask));
#if DCHECK_IS_ON()
  DCHECK_LE(address_begin_, chosen_chunk);
  DCHECK_LE(chosen_chunk + required_size, address_end_);
#endif
  return reinterpret_cast<void*>(chosen_chunk);
}

void AddressPoolManager::Pool::FreeChunk(uintptr_t address, size_t free_size) {
  base::AutoLock scoped_lock(lock_);

  const uintptr_t align_offset_mask = align_ - 1;
  DCHECK(!(address & align_offset_mask));

  const size_t size = bits::Align(free_size, align_);
#if DCHECK_IS_ON()
  DCHECK_LE(address_begin_, address);
  DCHECK_LE(address + size, address_end_);
#endif
  DCHECK_LT(address, address + size);
  auto new_chunk = std::make_pair(address, size);

  auto lower_bound = free_chunks_.lower_bound(address);
  if (lower_bound != free_chunks_.begin()) {
    auto left = --lower_bound;
    uintptr_t left_chunk_end = left->first + left->second;
    DCHECK_LE(left_chunk_end, address);
    if (left_chunk_end == address) {
      new_chunk.first = left->first;
      new_chunk.second += left->second;
      free_chunks_.erase(left);
    }
  }
  auto right = free_chunks_.upper_bound(address);
  if (right != free_chunks_.end()) {
    uintptr_t chunk_end = address + size;
    DCHECK_LE(chunk_end, right->first);
    if (right->first == chunk_end) {
      new_chunk.second += right->second;
      free_chunks_.erase(right);
    }
  }
  bool newly_inserted = free_chunks_.insert(new_chunk).second;
  DCHECK(newly_inserted);
}

AddressPoolManager::Pool::~Pool() = default;

AddressPoolManager::AddressPoolManager() = default;
AddressPoolManager::~AddressPoolManager() = default;

#endif  // defined(ARCH_CPU_64_BITS)

}  // namespace internal
}  // namespace base
