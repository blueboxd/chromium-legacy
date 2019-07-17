// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_WRITER_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_WRITER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_handle_base.h"
#include "content/common/content_export.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_writer.mojom.h"

namespace content {

// This is the browser side implementation of the
// NativeFileSystemFileWriter mojom interface. Instances of this class are
// owned by the NativeFileSystemManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods should be called on the IO thread.
// The link to the IO thread is due to its dependencies on both the blob system
// (via storage::BlobStorageContext) and the file system backends (via
// storage::FileSystemContext and storage::FileSystemOperationRunner, which both
// expect some of their methods to always be called on the IO thread).
// See https://crbug.com/957249 for some thoughts about the blob system aspect
// of this.
class CONTENT_EXPORT NativeFileSystemFileWriterImpl
    : public NativeFileSystemHandleBase,
      public blink::mojom::NativeFileSystemFileWriter {
 public:
  NativeFileSystemFileWriterImpl(NativeFileSystemManagerImpl* manager,
                                 const BindingContext& context,
                                 const storage::FileSystemURL& url,
                                 const SharedHandleState& handle_state);
  ~NativeFileSystemFileWriterImpl() override;
  void Write(uint64_t offset,
             blink::mojom::BlobPtr data,
             WriteCallback callback) override;
  void WriteStream(uint64_t offset,
                   mojo::ScopedDataPipeConsumerHandle stream,
                   WriteStreamCallback callback) override;

  void Truncate(uint64_t length, TruncateCallback callback) override;
  void Close(CloseCallback callback) override;

 private:
  // State that is kept for the duration of a write operation, to keep track of
  // progress until the write completes.
  struct WriteState;

  void WriteImpl(uint64_t offset,
                 blink::mojom::BlobPtr data,
                 WriteCallback callback);
  void DoWriteBlob(WriteCallback callback,
                   uint64_t position,
                   std::unique_ptr<storage::BlobDataHandle> blob);
  void WriteStreamImpl(uint64_t offset,
                       mojo::ScopedDataPipeConsumerHandle stream,
                       WriteStreamCallback callback);
  void DidWrite(WriteState* state,
                base::File::Error result,
                int64_t bytes,
                bool complete);
  void TruncateImpl(uint64_t length, TruncateCallback callback);
  void CloseImpl(CloseCallback callback);

  base::WeakPtr<NativeFileSystemHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<NativeFileSystemFileWriterImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemFileWriterImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_WRITER_IMPL_H_
