// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_PRODUCER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_PRODUCER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// Helper class which takes ownership of a ScopedDataPipeProducerHandle and
// assumes responsibility for feeding it the contents of a DataSource. This
// takes care of waiting for pipe capacity as needed, and can notify callers
// asynchronously when the operation is complete.
//
// Note that the DataPipeProducer must be kept alive until notified of
// completion to ensure that all of the intended contents are written to the
// pipe. Premature destruction may result in partial or total truncation of data
// made available to the consumer.
class MOJO_CPP_SYSTEM_EXPORT DataPipeProducer {
 public:
  using CompletionCallback = base::OnceCallback<void(MojoResult result)>;

  // Interface definition of an optional object that may be supplied to the
  // DataPipeProducer so that the data being read from the DataSource can be
  // observed.
  // TODO(crbug.com/983023): Remove Observer and achieve the same goal with
  // FilteredDataSource.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called once per read attempt. |data| contains the read data (if any).
    // |num_bytes_read| is the number of read bytes, 0 indicates EOF. Both
    // parameters may only be used when |read_result| is base::File::FILE_OK.
    // Can be called on any sequence.
    // TODO(crbug.com/983023): Consider to use MojoResult.
    virtual void OnBytesRead(const void* data,
                             size_t num_bytes_read,
                             base::File::Error read_result) = 0;

    // Called when the DataPipeProducer has finished reading all data. Will be
    // called even if there was an error opening the file or reading the data.
    // Can be called on any sequence.
    virtual void OnDoneReading() = 0;
  };

  // Interface definition of abstracted content reader that has minimum
  // base::File equivalent interface to read content.
  class DataSource {
   public:
    // Used as a return value for Read().
    struct ReadResult {
      // The number of bytes read. If returned |bytes_read| is less than
      // requested size, it means EOF is reached.
      size_t bytes_read = 0;

      // Error resulting from this call.
      // TODO(crbug.com/983023): Consider to use MojoResult.
      base::File::Error error = base::File::FILE_OK;
    };
    virtual ~DataSource() {}

    // Checks if this Reader instance owns a valid content to read.
    virtual bool IsValid() const = 0;

    // Similar to base::File::Read(), reads the given number of bytes (or until
    // EOF is reached) starting with the given offset. Returns ReadResult to
    // represent the number of bytes read and errors.
    virtual ReadResult Read(int64_t offset, base::span<char> buffer) = 0;
  };

  // Constructs a new DataPipeProducer which will write data to |producer|.
  // Caller may supply an optional |observer| if observation of the read data is
  // desired.
  DataPipeProducer(ScopedDataPipeProducerHandle producer,
                   std::unique_ptr<Observer> observer);
  ~DataPipeProducer();

  // Attempts to eventually write all of |data_source|'s contents to the pipe.
  // Invokes |callback| asynchronously when done. Note that |callback| IS
  // allowed to delete this DataPipeProducer.
  //
  // If the write is successful |result| will be |MOJO_RESULT_OK|. Otherwise
  // (e.g. if the producer detects the consumer is closed and the pipe has no
  // remaining capacity, or if file open/reads fail for any reason) |result|
  // will be one of the following:
  //
  //   |MOJO_RESULT_ABORTED|
  //   |MOJO_RESULT_NOT_FOUND|
  //   |MOJO_RESULT_PERMISSION_DENIED|
  //   |MOJO_RESULT_RESOURCE_EXHAUSTED|
  //   |MOJO_RESULT_UNKNOWN|
  //
  // Note that if the DataPipeProducer is destroyed before |callback| can be
  // invoked, |callback| is *never* invoked, and the write will be permanently
  // interrupted (and the producer handle closed) after making potentially only
  // partial progress.
  //
  // Multiple writes may be performed in sequence (each one after the last
  // completes), but Write() must not be called before the |callback| for the
  // previous call to Write() (if any) has returned.
  void Write(std::unique_ptr<DataSource> reader, CompletionCallback callback);

 private:
  class SequenceState;

  void InitializeNewRequest(CompletionCallback callback);
  void OnWriteComplete(CompletionCallback callback,
                       ScopedDataPipeProducerHandle producer,
                       MojoResult result);

  ScopedDataPipeProducerHandle producer_;
  scoped_refptr<SequenceState> sequence_state_;
  std::unique_ptr<Observer> observer_;
  base::WeakPtrFactory<DataPipeProducer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataPipeProducer);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_PRODUCER_H_
