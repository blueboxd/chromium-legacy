// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_SOURCE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_SOURCE_H_

#include <limits>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// A class to wrap base::File as DataPipeProducer::DataSource class. Reads at
// most |max_bytes| bytes from the file.
class MOJO_CPP_SYSTEM_EXPORT FileDataSource final
    : public DataPipeProducer::DataSource {
 public:
  static MojoResult ConvertFileErrorToMojoResult(base::File::Error error);

  FileDataSource(base::File file,
                 size_t max_bytes = std::numeric_limits<size_t>::max());
  ~FileDataSource() override;

 private:
  // DataPipeProducer::DataSource:
  bool IsValid() const override;
  ReadResult Read(int64_t offset, base::span<char> buffer) override;

  base::File file_;
  size_t max_bytes_;
  int64_t base_offset_;

  DISALLOW_COPY_AND_ASSIGN(FileDataSource);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_FILE_DATA_SOURCE_H_
