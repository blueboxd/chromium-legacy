// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/local_file_reader.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace drive {
namespace util {

LocalFileReader::LocalFileReader(
    base::SequencedTaskRunner* sequenced_task_runner)
    : file_stream_(sequenced_task_runner) {}

LocalFileReader::~LocalFileReader() = default;

void LocalFileReader::Open(const base::FilePath& file_path,
                           int64_t offset,
                           net::CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_ASYNC;

  int rv = file_stream_.Open(
      file_path, flags,
      BindOnce(&LocalFileReader::DidOpen, weak_ptr_factory_.GetWeakPtr(),
               std::move(callback), offset));
  DCHECK_EQ(rv, net::ERR_IO_PENDING);
}

void LocalFileReader::Read(net::IOBuffer* in_buffer,
                           int buffer_length,
                           net::CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(file_stream_.IsOpen());
  int rv = file_stream_.Read(in_buffer, buffer_length, std::move(callback));
  DCHECK_EQ(rv, net::ERR_IO_PENDING);
}

void LocalFileReader::DidOpen(net::CompletionOnceCallback callback,
                              int64_t offset,
                              int error) {
  if (error != net::OK)
    return std::move(callback).Run(error);

  int rv = file_stream_.Seek(offset, BindOnce(&LocalFileReader::DidSeek,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              std::move(callback), offset));
  DCHECK_EQ(rv, net::ERR_IO_PENDING);
}

void LocalFileReader::DidSeek(net::CompletionOnceCallback callback,
                              int64_t offset,
                              int64_t error) {
  std::move(callback).Run(error == offset ? net::OK : net::ERR_FAILED);
}

}  // namespace util
}  // namespace drive
