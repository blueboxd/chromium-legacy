// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/update_service_out_of_process.h"

#import <Foundation/Foundation.h>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_policy.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "chrome/updater/mac/xpc_service_names.h"
#import "chrome/updater/server/mac/service_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace {

// ScopedXPCUpdateServiceMock sets up mocks for the XPC Updater Service and
// provides access to those mocks. Mocks are removed when the object is
// deallocated. Only one of these objects can exist at a time, although
// it is not a singleton, since creating and destroying the mock manager
// is part of its behavior.
class ScopedXPCUpdateServiceMock {
 public:
  // RemoteObjectMockRecord represents a single call to a mock NSConnection's
  // remoteObjectProxyWithErrorHandler: or remoteObjectProxy method.
  // It contains a reference to the mock created in reply to the call,
  // and the block provided as an error handler. If no block was provided
  // (including when .remoteObjectProxy is used, which cannot accept any
  // error handler block argument), xpcErrorHandler will be nullopt.
  struct RemoteObjectMockRecord {
    // The mock object that will be served. Use this to configure behaviors
    // and expectations for the test.
    const base::scoped_nsprotocol<id<CRUUpdateChecking>> mock_object;

    // The error handler provided when the remote object was requested by the
    // code under test. Will not be populated before that request is issued.
    // If the remote object is requested via .remoteObjectProxy rather than
    // remoteObjectProxyWithErrorHandler:, this field is never populated.
    base::Optional<base::mac::ScopedBlock<void (^)(NSError*)>>
        xpc_error_handler;

    RemoteObjectMockRecord(
        base::scoped_nsprotocol<id<CRUUpdateChecking>> mock_ptr)
        : mock_object(mock_ptr) {}
  };

  // ConnectionMockRecord encapsulates a mock NSXPCConnection and the objects
  // that will be served across that connection to consecutive calls.
  class ConnectionMockRecord {
   public:
    // Constructs a ConnectionMockRecord to be served in the specified order.
    // This constructor is not intended for use outside of
    // ScopedXPCUpdateServiceMock. It produces a mock connection that is ready
    // for use, but has no associated mock remote objects.
    // It requires a pointer to its enclosing ScopedXPCUpdateServiceMock to
    // maintain correct behavior of mocked alloc operations.
    ConnectionMockRecord(ScopedXPCUpdateServiceMock* mock_driver, size_t index);
    ~ConnectionMockRecord() = default;

    ConnectionMockRecord(const ConnectionMockRecord& other) = delete;
    ConnectionMockRecord& operator=(const ConnectionMockRecord& other) = delete;
    ConnectionMockRecord(ConnectionMockRecord&& other) = delete;
    ConnectionMockRecord& operator=(ConnectionMockRecord&& other) = delete;

    // Gets the underlying connection mock itself (Objective-C flavor).
    // The mock is retained by |this|, and is not added to any autorelease
    // pools in response to this call.
    id Get();

    // Allocates a mock remote object, the next one to serve on this mocked
    // connection.
    //
    // Returns a pointer to the created RemoteObjectMockRecord, with |index| and
    // |mock_object| initialized. |xpc_error_handler| is guaranteed to be empty
    // when the record is created.
    //
    // The returned pointer is valid exactly as long as |this| is valid.
    RemoteObjectMockRecord* PrepareNewMockRemoteObject();

    // Find an already-created mock object by connection index.
    // Returns nullptr if no such index exists.
    //
    // The returned pointer is valid exactly as long as |this| is valid.
    RemoteObjectMockRecord* GetRemoteObject(size_t object_index);
    const RemoteObjectMockRecord* GetRemoteObject(size_t object_index) const;

    // Number of remote object mocks created for this mock connection.
    size_t PreparedObjectsCount() const;
    // Number of remote object mocks provided to test code from this mock
    // connection.
    size_t VendedObjectsCount() const;

    // Where in the order of connections that will be served by the enclosing
    // mock this mock connection is.
    size_t Index() const;

    // Verify that this connection was initialized and torn down properly,
    // and verifies OCMock expectations set on mock remote objects.
    void Verify() const;

   private:
    // The index of |this| in ScopedXPCUpdateServiceMock::mocked_connections_.
    // Used in verification failure messages.
    const size_t index_;
    base::scoped_nsobject<id> mock_connection_;  // mock NSXPCConnection
    std::vector<std::unique_ptr<RemoteObjectMockRecord>> remote_object_mocks_;
    size_t next_mock_to_vend_ = 0;
  };  // class ConnectionMockRecord

  ScopedXPCUpdateServiceMock();
  ~ScopedXPCUpdateServiceMock() = default;

  ScopedXPCUpdateServiceMock(const ScopedXPCUpdateServiceMock&) = delete;
  ScopedXPCUpdateServiceMock& operator=(const ScopedXPCUpdateServiceMock&) =
      delete;
  ScopedXPCUpdateServiceMock(ScopedXPCUpdateServiceMock&&) = delete;
  ScopedXPCUpdateServiceMock& operator=(ScopedXPCUpdateServiceMock&&) = delete;

  // Prepares another mock connection to be served on a consecutive expected
  // call to +[NSXPCConnection alloc]. Returns the record for the new mock.
  ConnectionMockRecord* PrepareNewMockConnection();

  ConnectionMockRecord* GetConnection(size_t index);
  const ConnectionMockRecord* GetConnection(size_t index) const;

  // Number of connection mocks created.
  size_t PreparedConnectionsCount() const;
  // Number of connection mocks provided to test code.
  size_t VendedConnectionsCount() const;

  // Verify all expectations on all mocked connections and all remote objects
  // for those connections. Callers may find it useful to perform more detailed
  // verification of results beyond what OCMExpect can capture. Mocked
  // connections expect to be allocated, initialized, resumed, and invalidated.
  void VerifyAll();

 private:
  // Implement [NSXPCConnection alloc] during the mock, working with an
  // NSInvocation object. The call is expected to have 0 arguments. To provide
  // a return value for this invocation of alloc, alter |invocation|.
  //
  // This implementation constructs an NSXPCConnection prepared to yield a
  // sequence of mock remote objects as previously prepared by |this|, referring
  // to the next prepared mock connection in sequence. If not enough connections
  // have been prepared, this prepares one first.
  void HandleConnectionAlloc(NSInvocation* invocation);

  // All connection mocks we have currently created.
  std::vector<std::unique_ptr<ConnectionMockRecord>> mocked_connections_;
  size_t next_connection_to_vend_ = 0;

  base::scoped_nsobject<id> nsxpcconnection_class_mock_;
};  // class ScopedXPCUpdateServiceMock

ScopedXPCUpdateServiceMock::ScopedXPCUpdateServiceMock() {
  @autoreleasepool {
    // Each NSXPCConnection mock must re-mock alloc (when a new mock is created
    // for type T, it un-mocks all previous classmethod mocks on type T - this
    // behavior differs from the documentation). Add a stub for alloc that
    // calls GTEST_FAIL(), since the only way this alloc stub is reachable
    // is if it has not been replaced with an alloc stub from a prepared
    // connection mock.
    nsxpcconnection_class_mock_.reset(OCMClassMock([NSXPCConnection class]),
                                      base::scoped_policy::RETAIN);
    OCMStub(ClassMethod([nsxpcconnection_class_mock_.get() alloc]))
        .andDo(^(NSInvocation* invocation) {
          GTEST_FAIL() << "[NSXPCConnection alloc] called before any "
                          "connection mocks prepared";
        });
  }
}

void ScopedXPCUpdateServiceMock::HandleConnectionAlloc(
    NSInvocation* invocation) {
  ASSERT_TRUE(invocation);

  size_t conn_idx = next_connection_to_vend_++;
  ASSERT_GT(mocked_connections_.size(), conn_idx);

  id mock_connection_ptr = mocked_connections_[conn_idx]->Get();
  // alloc returns net retain count of 1; OCMock doesn't automatically do this
  // when using .andDo, only when using .andReturn. Retain the return value here
  // to provide this net RC 1 behavior.
  [mock_connection_ptr retain];
  // NSInvocation copies the value referenced by the pointer provided to
  // -setReturnValue (in this case, a pointer itself).
  [invocation setReturnValue:&mock_connection_ptr];
}

ScopedXPCUpdateServiceMock::ConnectionMockRecord::ConnectionMockRecord(
    ScopedXPCUpdateServiceMock* mock_driver,
    size_t index)
    : index_(index),
      mock_connection_(OCMClassMock([NSXPCConnection class]),
                       base::scoped_policy::RETAIN) {
  @autoreleasepool {
    id mock_connection = mock_connection_.get();  // local for convenience

    // Every time we create a new mock of type X, all class method stubs for
    // type X are dropped. This is different from the behavior stated
    // in OCMock documentation. As a workaround for this behavior, we must
    // recreate the +[NSXPCConnection alloc] stub every time we create
    // a new mock for NSXPCConnection.
    //
    // If an OCMock update breaks these tests in mysterious ways, this
    // workaround has probably outlived its usefulness.
    OCMStub(ClassMethod([mock_connection alloc]))
        .andDo(^(NSInvocation* invocation) {
          mock_driver->HandleConnectionAlloc(invocation);
        });

    // Expect this connection to be initialized with some service name.
    OCMExpect([mock_connection initWithMachServiceName:[OCMArg any] options:0])
        .andReturn(mock_connection);

    // Expect this connection to receive a correct declaration of the remote
    // interface: an NSXPCInterface configured with CRUUpdateChecking.
    id verifyRemoteInterface = [OCMArg checkWithBlock:^BOOL(id interfaceArg) {
      if (!interfaceArg)
        return NO;
      if (![interfaceArg isKindOfClass:[NSXPCInterface class]])
        return NO;
      NSXPCInterface* interface = interfaceArg;
      // This test does not verify that the interface is declared correctly
      // around methods that require proxying or collection whitelisting.
      // Use end-to-end tests to detect this.
      return [interface.protocol isEqual:@protocol(CRUUpdateChecking)];
    }];
    OCMExpect([mock_connection setRemoteObjectInterface:verifyRemoteInterface]);

    // When remote objects are requested from this connection, they will come
    // from conn_rec_ptr->remote_object_mocks. If an error handler is
    // provided, we must populate the corresponding RemoteObjectMockRecord's
    // xpc_error_handler field. If we have run out of mock remote objects,
    // we must create a new one (just like if we run out of connections).
    OCMStub([mock_connection remoteObjectProxyWithErrorHandler:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          ASSERT_TRUE(invocation);
          ASSERT_LT(next_mock_to_vend_, remote_object_mocks_.size());
          RemoteObjectMockRecord* const mock_rec_ptr =
              remote_object_mocks_[next_mock_to_vend_++].get();
          id block_ptr;
          // Extract the error handler block. Objective-C Runtime argument
          // numbering convention starts with 0=self and 1=_cmd (the selector
          // for the current invocation); index 2 is the first "standard"
          // argument. NSInvocation is not type safe and it is very difficult to
          // derive the type of a block, so we'll just have to rely on
          // EXC_BAD_ACCESS if something that isn't even a block goes here.
          [invocation getArgument:&block_ptr atIndex:2];
          mock_rec_ptr->xpc_error_handler =
              base::mac::ScopedBlock<void (^)(NSError*)>(block_ptr);
          // Copy the mock remote object pointer so it has an address, since
          // -[NSInvocation setReturnValue:] requires indirection.
          id mock_remote_object_ptr = mock_rec_ptr->mock_object.get();
          [invocation setReturnValue:&mock_remote_object_ptr];
        });
    OCMStub([mock_connection remoteObjectProxy])
        .andDo(^(NSInvocation* invocation) {
          ASSERT_TRUE(invocation);
          ASSERT_LT(next_mock_to_vend_, remote_object_mocks_.size());
          RemoteObjectMockRecord* const mock_rec_ptr =
              remote_object_mocks_[next_mock_to_vend_++].get();
          // Copy the mock remote object pointer so it has an address, since
          // -[NSInvocation setReturnValue:] requires indirection.
          id mock_remote_object_ptr = mock_rec_ptr->mock_object.get();
          [invocation setReturnValue:&mock_remote_object_ptr];
        });

    // We don't expect to suspend connections, but we do have to resume them.
    OCMStub([(NSXPCConnection*)mock_connection suspend])
        .andDo(^(NSInvocation*) {
          GTEST_FAIL() << "suspend not implemented on mock NSXPCConnection.";
        });
    OCMExpect([(NSXPCConnection*)mock_connection resume]);

    // An NSXPCConnection must be invalidated before it is deallocated. Test
    // implementors take note: if this expectation fails, check the lifespan
    // of your object under test. If it's still alive during validation, this
    // will fail because the object hasn't cleaned up its connections yet.
    OCMExpect([mock_connection invalidate]);
  }  // autoreleasepool
}  // ScopedXPCUpdateServiceMock::ConnectionMockRecord constructor

id ScopedXPCUpdateServiceMock::ConnectionMockRecord::Get() {
  return mock_connection_.get();
}

ScopedXPCUpdateServiceMock::ConnectionMockRecord*
ScopedXPCUpdateServiceMock::PrepareNewMockConnection() {
  mocked_connections_.push_back(
      std::make_unique<ConnectionMockRecord>(this, mocked_connections_.size()));
  return mocked_connections_.back().get();
}

void ScopedXPCUpdateServiceMock::VerifyAll() {
  for (const auto& connection_ptr : mocked_connections_) {
    connection_ptr->Verify();
  }
}

size_t ScopedXPCUpdateServiceMock::PreparedConnectionsCount() const {
  return mocked_connections_.size();
}

size_t ScopedXPCUpdateServiceMock::VendedConnectionsCount() const {
  return next_connection_to_vend_;
}

ScopedXPCUpdateServiceMock::ConnectionMockRecord*
ScopedXPCUpdateServiceMock::GetConnection(size_t idx) {
  if (idx >= mocked_connections_.size()) {
    return nullptr;
  }
  return mocked_connections_[idx].get();
}

const ScopedXPCUpdateServiceMock::ConnectionMockRecord*
ScopedXPCUpdateServiceMock::GetConnection(size_t idx) const {
  if (idx >= mocked_connections_.size()) {
    return nullptr;
  }
  return const_cast<const ConnectionMockRecord*>(
      mocked_connections_[idx].get());
}

size_t ScopedXPCUpdateServiceMock::ConnectionMockRecord::PreparedObjectsCount()
    const {
  return remote_object_mocks_.size();
}

size_t ScopedXPCUpdateServiceMock::ConnectionMockRecord::VendedObjectsCount()
    const {
  return next_mock_to_vend_;
}

size_t ScopedXPCUpdateServiceMock::ConnectionMockRecord::Index() const {
  return index_;
}

ScopedXPCUpdateServiceMock::RemoteObjectMockRecord*
ScopedXPCUpdateServiceMock::ConnectionMockRecord::PrepareNewMockRemoteObject() {
  base::scoped_nsprotocol<id<CRUUpdateChecking>> mock(
      OCMProtocolMock(@protocol(CRUUpdateChecking)),
      base::scoped_policy::RETAIN);
  remote_object_mocks_.push_back(
      std::make_unique<RemoteObjectMockRecord>(mock));
  return remote_object_mocks_.back().get();
}

ScopedXPCUpdateServiceMock::RemoteObjectMockRecord*
ScopedXPCUpdateServiceMock::ConnectionMockRecord::GetRemoteObject(
    size_t object_index) {
  if (object_index >= remote_object_mocks_.size()) {
    return nullptr;
  }
  return remote_object_mocks_[object_index].get();
}

const ScopedXPCUpdateServiceMock::RemoteObjectMockRecord*
ScopedXPCUpdateServiceMock::ConnectionMockRecord::GetRemoteObject(
    size_t object_index) const {
  if (object_index >= remote_object_mocks_.size()) {
    return nullptr;
  }
  return const_cast<const RemoteObjectMockRecord*>(
      remote_object_mocks_[object_index].get());
}

void ScopedXPCUpdateServiceMock::ConnectionMockRecord::Verify() const {
  EXPECT_OCMOCK_VERIFY(mock_connection_.get())
      << "Verification failure in connection " << index_;
  for (size_t idx = 0; idx < remote_object_mocks_.size(); ++idx) {
    EXPECT_OCMOCK_VERIFY(remote_object_mocks_[idx]->mock_object.get())
        << "Verification failure in object " << idx << " on connection "
        << index_;
  }
}

// OCMockBlockCapturer<B> stores blocks provided as arguments to a method
// mocked from OCMock. Due to limitations in objective-C, its type checking
// is not good, so if attempts to call blocks captured by this object
// crash, you probably got a block of an unexpected type.
template <typename B>
class OCMockBlockCapturer {
 public:
  OCMockBlockCapturer() = default;
  OCMockBlockCapturer(const OCMockBlockCapturer&) = delete;
  OCMockBlockCapturer& operator=(const OCMockBlockCapturer&) = delete;
  OCMockBlockCapturer(OCMockBlockCapturer&&) = default;
  OCMockBlockCapturer& operator=(OCMockBlockCapturer&&) = default;
  ~OCMockBlockCapturer() = default;

  // Returns the captured blocks, in capture order.
  const std::vector<base::mac::ScopedBlock<B>>& Get() const { return blocks_; }

  // Retrieves an OCMArg that will store blocks passed when the method is
  // invoked. The OCMArg matches any argument passed to the mock.
  id Capture() {
    if (!arg_.get()) {
      arg_.reset([OCMArg checkWithBlock:^BOOL(id value) {
                   base::mac::ScopedBlock<B> wrapped(
                       static_cast<B>(value), base::scoped_policy::RETAIN);
                   blocks_.push_back(wrapped);
                   return YES;
                 }],
                 base::scoped_policy::RETAIN);
    }
    return (id)arg_.get();
  }

 private:
  std::vector<base::mac::ScopedBlock<B>> blocks_;
  base::scoped_nsobject<OCMArg> arg_;
};  // template class OCMockBlockCapturer

// A generic function template that ignores arguments and calls GTEST_FAIL.
// |msg| will be logged when this is called. Arguments are not printed (since
// unexpected values may not be loggable and may not exist).
template <typename... Ts>
void ExpectNoCalls(std::string msg, Ts... nope) {
  GTEST_FAIL() << msg;
}

}  // namespace

namespace updater {

class MacUpdateServiceOutOfProcessTest : public ::testing::Test {
 protected:
  void SetUp() override;

  // Create an UpdateServiceOutOfProcess and store in service_. Must be
  // called only on the task_environment_ sequence. SetUp() posts it.
  void InitializeService();

  base::test::SingleThreadTaskEnvironment task_environment_;
  ScopedXPCUpdateServiceMock mock_driver_;
  std::unique_ptr<base::RunLoop> run_loop_;
  scoped_refptr<UpdateServiceOutOfProcess> service_;
};  // class MacUpdateOutOfProcessTest

void MacUpdateServiceOutOfProcessTest::SetUp() {
  run_loop_ = std::make_unique<base::RunLoop>();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        service_ = base::MakeRefCounted<UpdateServiceOutOfProcess>(
            UpdateService::Scope::kUser);
      }));
}

TEST_F(MacUpdateServiceOutOfProcessTest, NoProductsUpdateAll) {
  ScopedXPCUpdateServiceMock::ConnectionMockRecord* conn_rec =
      mock_driver_.PrepareNewMockConnection();
  ScopedXPCUpdateServiceMock::RemoteObjectMockRecord* mock_rec =
      conn_rec->PrepareNewMockRemoteObject();
  id<CRUUpdateChecking> mock_remote_object = mock_rec->mock_object.get();

  OCMockBlockCapturer<void (^)(UpdateService::Result)> reply_block_capturer;
  // Create a pointer that can be copied into the .andDo block to refer to the
  // capturer so we can invoke the captured block.
  auto* reply_block_capturer_ptr = &reply_block_capturer;
  OCMExpect([mock_remote_object
                checkForUpdatesWithUpdateState:[OCMArg isNotNil]
                                         reply:reply_block_capturer.Capture()])
      .andDo(^(NSInvocation*) {
        reply_block_capturer_ptr->Get()[0].get()(
            UpdateService::Result::kAppNotFound);
      });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        service_->UpdateAll(
            base::BindRepeating(&ExpectNoCalls<UpdateService::UpdateState>,
                                "no state updates expected"),
            base::BindLambdaForTesting(
                [this](UpdateService::Result actual_result) {
                  EXPECT_EQ(UpdateService::Result::kAppNotFound, actual_result);
                  run_loop_->QuitWhenIdle();
                }));
      }));

  run_loop_->Run();

  EXPECT_EQ(reply_block_capturer.Get().size(), 1UL);
  EXPECT_EQ(conn_rec->VendedObjectsCount(), 1UL);
  EXPECT_EQ(mock_driver_.VendedConnectionsCount(), 1UL);
  service_.reset();  // Drop service reference - should invalidate connection
  mock_driver_.VerifyAll();
}

}  // namespace updater
