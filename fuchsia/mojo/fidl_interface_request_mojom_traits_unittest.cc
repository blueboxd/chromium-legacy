// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/testfidl/cpp/fidl.h"
#include "base/message_loop/message_loop.h"
#include "fuchsia/mojo/example.mojom.h"
#include "fuchsia/mojo/test_interface_request_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuchsia {

using base::fuchsia::testfidl::TestInterface;
using base::fuchsia::testfidl::TestInterfacePtr;

TEST(InterfaceRequestStructTraitsTest, Serialization) {
  base::MessageLoopForIO message_loop;
  TestInterfacePtr test_ptr;
  fidl::InterfaceRequest<TestInterface> input_request = test_ptr.NewRequest();
  fidl::InterfaceRequest<TestInterface> output_request;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              fuchsia::test::mojom::TestInterfaceRequest>(&input_request,
                                                          &output_request));
  EXPECT_TRUE(output_request.is_valid());
}

}  // namespace fuchsia
