// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/mojo_data_pump.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MojoDataPumpTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Environment must be set up first
    task_environment = std::make_unique<base::test::TaskEnvironment>();

    mojo::ScopedDataPipeProducerHandle send_pipe_producer;
    mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer;
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &send_pipe_producer,
                                                   &send_pipe_consumer));
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, &receive_pipe_producer,
                                   &receive_pipe_consumer));
    pump = std::make_unique<MojoDataPump>(std::move(receive_pipe_consumer),
                                          std::move(send_pipe_producer));
  }

  std::unique_ptr<MojoDataPump> pump;
  mojo::ScopedDataPipeProducerHandle receive_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer;
  std::unique_ptr<base::test::TaskEnvironment> task_environment;
};

TEST_F(MojoDataPumpTest, CanReceiveRepeatedly) {
  base::RunLoop run_loop;
  std::string data("12345");
  std::string second_write_data("67890");
  int callback_called_count = 0;
  auto io_buffer = base::MakeRefCounted<net::IOBuffer>(11ul);
  pump->Read(io_buffer.get(), 11ul, base::BindLambdaForTesting([&](int result) {
               callback_called_count++;
               // We should be called only once, when both writes are complete.
               ASSERT_EQ(10, result);
               EXPECT_EQ(data + second_write_data,
                         std::string(io_buffer->data(), result));
               run_loop.Quit();
             }));

  uint32_t num_bytes = data.size();
  // WriteData() completes synchronously because |data| is much smaller than
  // data pipe's internal buffer.
  MojoResult result = receive_pipe_producer->WriteData(
      data.data(), &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(data.size(), num_bytes);

  uint32_t second_num_bytes = second_write_data.size();
  MojoResult second_result = receive_pipe_producer->WriteData(
      second_write_data.data(), &second_num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, second_result);
  ASSERT_EQ(second_write_data.size(), second_num_bytes);

  // Now pump->Read() should complete.
  run_loop.Run();
  EXPECT_EQ(callback_called_count, 1);
}

TEST_F(MojoDataPumpTest, CanSendRepeatedly) {
  base::RunLoop run_loop;
  scoped_refptr<net::StringIOBuffer> write_buffer =
      base::MakeRefCounted<net::StringIOBuffer>("dummy");
  net::TestCompletionCallback callback;
  pump->Write(write_buffer.get(), write_buffer->size(), callback.callback());
  EXPECT_EQ(write_buffer->size(), callback.WaitForResult());

  scoped_refptr<net::StringIOBuffer> second_write_buffer =
      base::MakeRefCounted<net::StringIOBuffer>("second");
  net::TestCompletionCallback second_callback;
  pump->Write(second_write_buffer.get(), second_write_buffer->size(),
              second_callback.callback());
  EXPECT_EQ(second_write_buffer->size(), second_callback.WaitForResult());

  char buffer[16] = {};
  uint32_t bytes_read = 16;
  send_pipe_consumer->ReadData(buffer, &bytes_read, MOJO_READ_DATA_FLAG_NONE);
  EXPECT_STREQ(buffer, "dummysecond");
}

// Tests that if |MojoDataPump::receive_stream_| is not ready, MojoDataPump will
// wait and not error out.
TEST_F(MojoDataPumpTest, ReceiveStreamNotReady) {
  base::RunLoop run_loop;
  std::string data("dummy");
  bool callback_called = false;
  auto io_buffer = base::MakeRefCounted<net::IOBuffer>(10ul);
  pump->Read(io_buffer.get(), 10ul, base::BindLambdaForTesting([&](int result) {
               callback_called = true;
               ASSERT_EQ(static_cast<int>(data.size()), result);
               EXPECT_EQ(data, std::string(io_buffer->data(), result));
               run_loop.Quit();
             }));

  // Spin the message loop so that MojoDataPump::ReceiveMore() is called but the
  // callback will not be executed yet because there is no data to read.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_called);

  uint32_t num_bytes = data.size();
  // WriteData() completes synchronously because |data| is much smaller than
  // data pipe's internal buffer.
  MojoResult r = receive_pipe_producer->WriteData(data.data(), &num_bytes,
                                                  MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, r);
  ASSERT_EQ(data.size(), num_bytes);

  // Now pump->Read() should complete.
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

// Tests that if |MojoDataPump::receive_stream_| is closed, an error is
// propagated.
TEST_F(MojoDataPumpTest, ReceiveStreamClosed) {
  base::RunLoop run_loop;
  auto io_buffer = base::MakeRefCounted<net::IOBuffer>(10ul);
  pump->Read(io_buffer.get(), 10ul, base::BindLambdaForTesting([&](int result) {
               EXPECT_EQ(net::ERR_FAILED, result);
               run_loop.Quit();
             }));

  receive_pipe_producer.reset();

  // Now pump->Read() should complete.
  run_loop.Run();
}

// Tests that if |MojoDataPump::send_stream_| is closed, Write() will fail.
TEST_F(MojoDataPumpTest, SendStreamClosed) {
  scoped_refptr<net::StringIOBuffer> write_buffer =
      base::MakeRefCounted<net::StringIOBuffer>("dummy");
  net::TestCompletionCallback callback;
  send_pipe_consumer.reset();
  pump->Write(write_buffer.get(), write_buffer->size(), callback.callback());
  EXPECT_EQ(net::ERR_FAILED, callback.WaitForResult());
}

}  // namespace media_router
