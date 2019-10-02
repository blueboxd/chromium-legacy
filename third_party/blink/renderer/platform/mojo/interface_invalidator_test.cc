// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/interfaces/bindings/tests/ping_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/mojo/interface_invalidator.h"
#include "third_party/blink/renderer/platform/mojo/revocable_interface_ptr.h"

namespace blink {

namespace {

void DoSetFlag(bool* flag) {
  *flag = true;
}

class PingServiceImplBase : public mojo::test::blink::PingService {
 public:
  PingServiceImplBase(bool send_response = true)
      : send_response_(send_response) {}
  ~PingServiceImplBase() override {}

  // mojo::test::blink::PingService:
  void Ping(PingCallback callback) override {
    if (ping_handler_)
      ping_handler_.Run();

    if (send_response_) {
      std::move(callback).Run();
    } else {
      saved_callback_ = std::move(callback);
    }

    if (post_ping_handler_)
      post_ping_handler_.Run();
  }

  void set_ping_handler(const base::RepeatingClosure& handler) {
    ping_handler_ = handler;
  }

  void set_post_ping_handler(const base::RepeatingClosure& handler) {
    post_ping_handler_ = handler;
  }

 private:
  bool send_response_;
  PingCallback saved_callback_;
  base::RepeatingClosure ping_handler_;
  base::RepeatingClosure post_ping_handler_;

  DISALLOW_COPY_AND_ASSIGN(PingServiceImplBase);
};

class PingServiceImpl : public PingServiceImplBase {
 public:
  PingServiceImpl(
      mojo::PendingReceiver<mojo::test::blink::PingService> receiver,
      bool send_response = true)
      : PingServiceImplBase(send_response),
        error_handler_called_(false),
        receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(DoSetFlag, &error_handler_called_));
  }

  ~PingServiceImpl() override {}

  bool error_handler_called() { return error_handler_called_; }

  mojo::Receiver<mojo::test::blink::PingService>* receiver() {
    return &receiver_;
  }

 private:
  bool error_handler_called_;
  mojo::Receiver<mojo::test::blink::PingService> receiver_;

  DISALLOW_COPY_AND_ASSIGN(PingServiceImpl);
};

class InterfaceInvalidatorTest : public testing::Test {
 public:
  InterfaceInvalidatorTest() {}
  ~InterfaceInvalidatorTest() override {}

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceInvalidatorTest);
};

class InterfaceInvalidatorObserver : public InterfaceInvalidator::Observer {
 public:
  InterfaceInvalidatorObserver(const base::RepeatingClosure& handler) {
    invalidate_handler_ = handler;
  }
  ~InterfaceInvalidatorObserver() {}

  void OnInvalidate() override { invalidate_handler_.Run(); }

 private:
  base::RepeatingClosure invalidate_handler_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceInvalidatorObserver);
};

TEST_F(InterfaceInvalidatorTest, DestroyNotifiesObservers) {
  int called = 0;
  auto inc_called_cb = base::BindLambdaForTesting([&] { ++called; });
  InterfaceInvalidatorObserver observer1(inc_called_cb);
  InterfaceInvalidatorObserver observer2(inc_called_cb);
  {
    InterfaceInvalidator invalidator;
    invalidator.AddObserver(&observer1);
    invalidator.AddObserver(&observer2);
    EXPECT_EQ(called, 0);
  }
  EXPECT_EQ(called, 2);
}

TEST_F(InterfaceInvalidatorTest, DestroyInvalidatesRevocableInterfacePtr) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  bool ping_called = false;
  wptr->Ping(base::BindRepeating(DoSetFlag, &ping_called));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(ping_called);

  bool error_handler_called = false;
  wptr.set_connection_error_handler(
      base::BindOnce(DoSetFlag, &error_handler_called));

  invalidator.reset();
  impl.set_ping_handler(base::BindRepeating([] { FAIL(); }));
  wptr->Ping(base::BindRepeating([] { FAIL(); }));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(error_handler_called);
  EXPECT_TRUE(impl.error_handler_called());
  EXPECT_TRUE(wptr.encountered_error());
  EXPECT_TRUE(wptr);
}

TEST_F(InterfaceInvalidatorTest, InvalidateAfterMessageSent) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  bool called = false;
  impl.set_ping_handler(base::BindRepeating(DoSetFlag, &called));
  // The passed in callback will not be called as the interface is invalidated
  // before a response can come back.
  wptr->Ping(base::BindRepeating([] { FAIL(); }));
  invalidator.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_TRUE(impl.error_handler_called());
}

TEST_F(InterfaceInvalidatorTest, PassInterfaceThenInvalidate) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  bool impl_called = false;
  impl.set_ping_handler(base::BindRepeating(DoSetFlag, &impl_called));
  wptr.set_connection_error_handler(base::BindOnce([] { FAIL(); }));

  mojo::test::blink::PingServicePtr ptr(wptr.PassInterface());
  invalidator.reset();
  bool ping_called = false;
  ptr->Ping(base::BindRepeating(DoSetFlag, &ping_called));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ping_called);
  EXPECT_TRUE(impl_called);
  EXPECT_FALSE(impl.error_handler_called());
}

TEST_F(InterfaceInvalidatorTest, PassInterfaceOfInvalidatedPtr) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  impl.set_ping_handler(base::BindRepeating([] { FAIL(); }));
  bool error_handler_called = false;
  wptr.set_connection_error_handler(
      base::BindOnce(DoSetFlag, &error_handler_called));

  // This also destroys the original invalidator.
  invalidator = std::make_unique<InterfaceInvalidator>();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(error_handler_called);
  ASSERT_TRUE(impl.error_handler_called());

  RevocableInterfacePtr<mojo::test::blink::PingService> wptr2(
      wptr.PassInterface(), invalidator.get(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  wptr2->Ping(base::BindRepeating([] { FAIL(); }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(InterfaceInvalidatorTest,
       PassInterfaceBeforeConnectionErrorNotification) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  impl.set_ping_handler(base::BindRepeating([] { FAIL(); }));
  wptr.set_connection_error_handler(base::BindOnce([] { FAIL(); }));

  // This also destroys the original invalidator.
  invalidator = std::make_unique<InterfaceInvalidator>();
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr2(
      wptr.PassInterface(), invalidator.get(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  wptr2->Ping(base::BindRepeating([] { FAIL(); }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(impl.error_handler_called());
}

TEST_F(InterfaceInvalidatorTest, InvalidateAfterReset) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));
  wptr.set_connection_error_handler(base::BindOnce([] { FAIL(); }));

  wptr.reset();
  invalidator.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(wptr);
}

TEST_F(InterfaceInvalidatorTest, ResetInvalidatedRevocableInterfacePtr) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));
  wptr.set_connection_error_handler(base::BindOnce([] { FAIL(); }));

  invalidator.reset();
  wptr.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(InterfaceInvalidatorTest, InvalidateErroredPtr) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  int called = 0;
  wptr.set_connection_error_handler(
      base::BindLambdaForTesting([&] { called++; }));

  impl.receiver()->reset();
  base::RunLoop().RunUntilIdle();
  invalidator.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(called, 1);
  EXPECT_FALSE(impl.error_handler_called());
}

// InterfacePtrs do not set up a proxy until they are used for the first
// time.
TEST_F(InterfaceInvalidatorTest, InvalidateBeforeProxyConfigured) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  invalidator.reset();
  wptr->Ping(base::BindRepeating([] { FAIL(); }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(impl.error_handler_called());
}

TEST_F(InterfaceInvalidatorTest, MoveChangesInvalidatorObserver) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  auto wptr2(std::move(wptr));
  bool called = false;
  wptr2.set_connection_error_handler(base::BindOnce(DoSetFlag, &called));

  invalidator.reset();
  wptr2->Ping(base::BindRepeating([] { FAIL(); }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_TRUE(impl.error_handler_called());
}

TEST_F(InterfaceInvalidatorTest, MoveInvalidatedPointer) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  invalidator.reset();
  auto wptr2(std::move(wptr));
  wptr2->Ping(base::BindRepeating([] { FAIL(); }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(InterfaceInvalidatorTest, InvalidateRevocableInterfacePtrDuringSyncIPC) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()));

  impl.set_ping_handler(
      base::BindLambdaForTesting([&]() { invalidator.reset(); }));
  bool result = wptr->Ping();
  EXPECT_FALSE(result);
}

TEST_F(InterfaceInvalidatorTest,
       InvalidateRevocableInterfacePtrDuringSyncIPCWithoutResponse) {
  RevocableInterfacePtr<mojo::test::blink::PingService> wptr;
  auto invalidator = std::make_unique<InterfaceInvalidator>();
  PingServiceImpl impl(MakeRequest(&wptr, invalidator.get()),
                       false /* send_response */);

  impl.set_ping_handler(
      base::BindLambdaForTesting([&]() { invalidator.reset(); }));
  bool result = wptr->Ping();
  EXPECT_FALSE(result);
}

}  // namespace

}  // namespace blink
