// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_per_frame_memory_reporter_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

class V8PerFrameMemoryReporterImplTest : public SimTest {};

namespace {

class MemoryUsageChecker {
 public:
  void Callback(mojom::blink::PerProcessV8MemoryUsageDataPtr result) {
    EXPECT_EQ(2u, result->associated_memory.size());
    for (const auto& frame_memory : result->associated_memory) {
      for (const auto& entry : frame_memory->associated_bytes) {
        EXPECT_EQ(0u, entry->world_id);
        EXPECT_LT(4000000u, entry->bytes_used);
      }
    }
    called_ = true;
  }
  bool IsCalled() { return called_; }

 private:
  bool called_ = false;
};

}  // anonymous namespace

TEST_F(V8PerFrameMemoryReporterImplTest, GetPerFrameV8MemoryUsageData) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://example.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(R"HTML(
      <script>
        window.onload = function () {
          globalThis.array = new Array(1000000).fill(0);
          console.log("main loaded");
        }
      </script>
      <body>
        <iframe src='https://example.com/subframe.html'></iframe>
      </body>)HTML"));

  test::RunPendingTasks();

  child_frame_resource.Complete(String::Format(R"HTML(
      <script>
        window.onload = function () {
          globalThis.array = new Array(1000000).fill(0);
          console.log("iframe loaded");
        }
      </script>
      <body>
      </body>)HTML"));

  test::RunPendingTasks();

  // Ensure that main frame and subframe are loaded before measuring memory
  // usage.
  EXPECT_TRUE(ConsoleMessages().Contains("main loaded"));
  EXPECT_TRUE(ConsoleMessages().Contains("iframe loaded"));

  V8PerFrameMemoryReporterImpl reporter;
  MemoryUsageChecker checker;
  reporter.GetPerFrameV8MemoryUsageData(
      V8PerFrameMemoryReporterImpl::Mode::EAGER,
      WTF::Bind(&MemoryUsageChecker::Callback, WTF::Unretained(&checker)));

  test::RunPendingTasks();

  EXPECT_TRUE(checker.IsCalled());
}
}  // namespace blink
