// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_ALLOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_ALLOCATOR_H_

#include <map>
#include <memory>

#include "base/files/file.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/disk_allocator.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

// Stores data onto a single file.
//
// The file is provided after construction. As a consequence, the allocator
// initially does not accept writes, that is |Write()| returns nullptr. It may
// also become not usable later, for instance if disk space is no longer
// available.
//
// Threading:
// - Reads must be done from the main thread
// - Writes can be done from any thread.
// - public methods are thread-safe, and unless otherwise noted, can be called
//   from any thread.
class PLATFORM_EXPORT DiskDataAllocator : public mojom::blink::DiskAllocator {
 public:
  class Metadata {
   public:
    int64_t start_offset() const { return start_offset_; }
    size_t size() const { return size_; }
    Metadata(Metadata&& other) = delete;

   private:
    Metadata(int64_t start_offset, size_t size)
        : start_offset_(start_offset), size_(size) {}
    Metadata(const Metadata& other) = default;
    Metadata& operator=(const Metadata& other) = default;

    int64_t start_offset_;
    size_t size_;

    friend class DiskDataAllocator;
  };

  // Must be called on the main thread.
  void ProvideTemporaryFile(::base::File file) override;

  // Whether writes may succeed. This is not a guarantee. However, when this
  // returns false, writes will fail.
  bool may_write() LOCKS_EXCLUDED(mutex_);

  // Returns |nullptr| in case of error.
  // Note that this performs a blocking disk write.
  std::unique_ptr<Metadata> Write(const void* data, size_t size);

  // Reads data. A read failure is fatal.
  // Must be called from the main thread.
  // Can be called at any time before |Discard()| destroys |metadata|.
  //
  // |data| must point to an area large enough to fit a |metadata.size|-ed
  // array. Note that this performs a blocking disk read.
  void Read(const Metadata& metadata, void* data);

  // Discards existing data pointed at by |metadata|.
  void Discard(std::unique_ptr<Metadata> metadata);

  ~DiskDataAllocator() override;
  static DiskDataAllocator& Instance();
  static void Bind(mojo::PendingReceiver<mojom::blink::DiskAllocator> receiver);

 protected:
  // Protected methods for testing.
  DiskDataAllocator();
  void set_may_write_for_testing(bool may_write) LOCKS_EXCLUDED(mutex_);

 private:
  Metadata FindChunk(size_t size) EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void ReleaseChunk(const Metadata& metadata) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Virtual for testing.
  virtual int DoWrite(int64_t offset, const char* data, int size)
      LOCKS_EXCLUDED(mutex_);
  // CHECK()s that the read is successful.
  virtual void DoRead(int64_t offset, char* data, int size);

  mojo::Receiver<mojom::blink::DiskAllocator> receiver_{this};
  base::File file_;  // May be invalid.

 protected:  // For testing.
  Mutex mutex_;
  // Using a std::map because we rely on |{lower,upper}_bound()|.
  std::map<int64_t, size_t> free_chunks_ GUARDED_BY(mutex_);
  size_t free_chunks_size_ GUARDED_BY(mutex_);

 private:
  int64_t file_tail_ GUARDED_BY(mutex_);
  // Whether writing is possible now. This can be true if:
  // - |set_may_write_for_testing()| was called, or
  // - |file_.IsValid()| and no write error occurred (which would set
  //   |may_write_| to false).
  bool may_write_ GUARDED_BY(mutex_);
#if DCHECK_IS_ON()
  std::map<int64_t, size_t> allocated_chunks_ GUARDED_BY(mutex_);
#endif

  FRIEND_TEST_ALL_PREFIXES(DiskDataAllocatorTest, ProvideInvalidFile);
  FRIEND_TEST_ALL_PREFIXES(DiskDataAllocatorTest, ProvideValidFile);
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_ALLOCATOR_H_
