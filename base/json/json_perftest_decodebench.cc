// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This program measures the time taken to decode the given JSON files (the
// command line arguments). It is for manual benchmarking.
//
// Usage:
// $ ninja -C out/foobar json_perftest_decodebench
// $ out/foobar/json_perftest_decodebench -a -n=10 the/path/to/your/*.json
//
// The -n=10 switch controls the number of iterations. It defaults to 1.
//
// The -a switch means to print 1 non-comment line per input file (the average
// iteration time). Without this switch (the default), it prints n non-comment
// lines per input file (individual iteration times). For a single input file,
// building and running this program before and after a particular commit can
// work well with the 'ministat' tool: https://github.com/thorduri/ministat

#include <inttypes.h>
#include <stdio.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"

int main(int argc, char* argv[]) {
  if (!base::ThreadTicks::IsSupported()) {
    printf("# base::ThreadTicks is not supported\n");
    return EXIT_FAILURE;
  }
  base::ThreadTicks::WaitUntilInitialized();

  base::CommandLine command_line(argc, argv);
  bool average = command_line.HasSwitch("a");
  int iterations = 1;
  std::string iterations_str = command_line.GetSwitchValueASCII("n");
  if (!iterations_str.empty()) {
    iterations = atoi(iterations_str.c_str());
    if (iterations < 1) {
      printf("# invalid -n command line switch\n");
      return EXIT_FAILURE;
    }
  }

  if (average) {
    printf("# Microseconds (μs), n=%d, averaged\n", iterations);
  } else {
    printf("# Microseconds (μs), n=%d\n", iterations);
  }
  for (const auto& filename : command_line.GetArgs()) {
    std::string src;
    if (!base::ReadFileToString(base::FilePath(filename), &src)) {
      printf("# could not read %s\n", filename.c_str());
      return EXIT_FAILURE;
    }
    int64_t total_time = 0;
    for (int i = 0; i < iterations; ++i) {
      auto start = base::ThreadTicks::Now();
      auto v = base::JSONReader::ReadAndReturnValueWithError(src);
      auto end = base::ThreadTicks::Now();
      int64_t iteration_time = (end - start).InMicroseconds();
      total_time += iteration_time;

      if (i == 0) {
        if (!v.error_message.empty()) {
          printf("# %s: %s\n", filename.c_str(), v.error_message.c_str());
        } else if (!average) {
          printf("# %s\n", filename.c_str());
        }
      }

      if (!average) {
        printf("%" PRId64 "\n", iteration_time);
      }
    }

    if (average) {
      int64_t average_time = total_time / iterations;
      printf("%12" PRId64 "\t# %s\n", average_time, filename.c_str());
    }
  }
  return EXIT_SUCCESS;
}
