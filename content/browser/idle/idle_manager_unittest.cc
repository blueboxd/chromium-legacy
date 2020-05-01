// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"

using blink::mojom::IdleManagerError;
using blink::mojom::IdleMonitorPtr;
using blink::mojom::IdleStatePtr;
using blink::mojom::ScreenIdleState;
using blink::mojom::UserIdleState;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using url::Origin;

namespace content {

namespace {

const char kTestUrl[] = "https://www.google.com";

constexpr base::TimeDelta kThreshold = base::TimeDelta::FromSeconds(60);

class MockIdleMonitor : public blink::mojom::IdleMonitor {
 public:
  MOCK_METHOD1(Update, void(IdleStatePtr));
};

class MockIdleTimeProvider : public IdleManager::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;
  ~MockIdleTimeProvider() override = default;
  MockIdleTimeProvider(const MockIdleTimeProvider&) = delete;
  MockIdleTimeProvider& operator=(const MockIdleTimeProvider&) = delete;

  MOCK_METHOD0(CalculateIdleTime, base::TimeDelta());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());
};

class IdleManagerTest : public RenderViewHostImplTestHarness {
 protected:
  IdleManagerTest() = default;
  ~IdleManagerTest() override = default;
  IdleManagerTest(const IdleManagerTest&) = delete;
  IdleManagerTest& operator=(const IdleManagerTest&) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    permission_manager_ = new ::testing::NiceMock<MockPermissionManager>();
    static_cast<TestBrowserContext*>(browser_context())
        ->SetPermissionControllerDelegate(
            base::WrapUnique(permission_manager_));
    idle_manager_ = std::make_unique<IdleManagerImpl>(browser_context());
  }

  void TearDown() override {
    idle_manager_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  IdleManagerImpl* GetIdleManager() { return idle_manager_.get(); }

  void SetPermissionStatus(const GURL& origin,
                           blink::mojom::PermissionStatus permission_status) {
    ON_CALL(*permission_manager_,
            GetPermissionStatus(PermissionType::NOTIFICATIONS, origin, origin))
        .WillByDefault(Return(permission_status));
  }

  Origin origin() const { return Origin::Create(url_); }
  const GURL& url() const { return url_; }

 private:
  std::unique_ptr<IdleManagerImpl> idle_manager_;
  MockPermissionManager* permission_manager_;
  GURL url_ = GURL(kTestUrl);
};

}  // namespace

TEST_F(IdleManagerTest, AddMonitor) {
  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  base::RunLoop loop;

  service_remote.set_disconnect_handler(base::BindLambdaForTesting([&]() {
    ADD_FAILURE() << "Unexpected connection error";
    loop.Quit();
  }));

  // Initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime())
      .WillRepeatedly(Return(base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(false));

  service_remote->AddMonitor(
      kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
      base::BindOnce(
          [](base::OnceClosure callback, IdleManagerError error,
             IdleStatePtr state) {
            // The initial state of the status of the user is to be active.
            EXPECT_EQ(IdleManagerError::kSuccess, error);
            EXPECT_EQ(UserIdleState::kActive, state->user);
            EXPECT_EQ(ScreenIdleState::kUnlocked, state->screen);
            std::move(callback).Run();
          },
          loop.QuitClosure()));

  loop.Run();
}

// Disabled test: https://crbug.com/1062668
TEST_F(IdleManagerTest, DISABLED_Idle) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;
    // Initial state of the system.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(Return(base::TimeDelta::FromSeconds(0)));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting(
            [&](IdleManagerError error, IdleStatePtr state) {
              EXPECT_EQ(IdleManagerError::kSuccess, error);
              EXPECT_EQ(UserIdleState::kActive, state->user);
              loop.Quit();
            }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going idle.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(Return(base::TimeDelta::FromSeconds(60)));

    // Expects Update to be notified about the change to idle.
    EXPECT_CALL(monitor, Update(_)).WillOnce(Invoke([&](IdleStatePtr state) {
      EXPECT_EQ(UserIdleState::kIdle, state->user);
      loop.Quit();
    }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going active, calling a callback under the threshold.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(Return(base::TimeDelta::FromSeconds(0)));

    // Expects Update to be notified about the change to active.
    // auto quit = loop.QuitClosure();
    EXPECT_CALL(monitor, Update(_)).WillOnce(Invoke([&](IdleStatePtr state) {
      EXPECT_EQ(UserIdleState::kActive, state->user);
      // Ends the test.
      loop.Quit();
    }));
    loop.Run();
  }
}

TEST_F(IdleManagerTest, UnlockingScreen) {
  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(true));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting(
            [&](IdleManagerError error, IdleStatePtr state) {
              EXPECT_EQ(IdleManagerError::kSuccess, error);
              EXPECT_EQ(ScreenIdleState::kLocked, state->screen);
              loop.Quit();
            }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user unlocking the screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(false));

    // Expects Update to be notified about the change to unlocked.
    EXPECT_CALL(monitor, Update(_)).WillOnce(Invoke([&](IdleStatePtr state) {
      EXPECT_EQ(ScreenIdleState::kUnlocked, state->screen);
      loop.Quit();
    }));

    loop.Run();
  }
}

// Disabled test: https://crbug.com/1062668
TEST_F(IdleManagerTest, DISABLED_LockingScreen) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(false));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting(
            [&](IdleManagerError error, IdleStatePtr state) {
              EXPECT_EQ(IdleManagerError::kSuccess, error);
              EXPECT_EQ(ScreenIdleState::kUnlocked, state->screen);
              loop.Quit();
            }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user locking the screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(true));

    // Expects Update to be notified about the change to unlocked.
    EXPECT_CALL(monitor, Update(_)).WillOnce(Invoke([&](IdleStatePtr state) {
      EXPECT_EQ(ScreenIdleState::kLocked, state->screen);
      loop.Quit();
    }));

    loop.Run();
  }
}

// Disabled test: https://crbug.com/1062668
TEST_F(IdleManagerTest, DISABLED_LockingScreenThenIdle) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(false));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting(
            [&](IdleManagerError error, IdleStatePtr state) {
              EXPECT_EQ(IdleManagerError::kSuccess, error);
              EXPECT_EQ(UserIdleState::kActive, state->user);
              EXPECT_EQ(ScreenIdleState::kUnlocked, state->screen);
              loop.Quit();
            }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user locking screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(true));

    // Expects Update to be notified about the change to locked.
    EXPECT_CALL(monitor, Update(_)).WillOnce(Invoke([&](IdleStatePtr state) {
      EXPECT_EQ(ScreenIdleState::kLocked, state->screen);
      EXPECT_EQ(UserIdleState::kActive, state->user);
      loop.Quit();
    }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user going idle, whilte the screen is still locked.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(true));

    // Expects Update to be notified about the change to active.
    EXPECT_CALL(monitor, Update(_)).WillOnce(Invoke([&](IdleStatePtr state) {
      EXPECT_EQ(UserIdleState::kIdle, state->user);
      EXPECT_EQ(ScreenIdleState::kLocked, state->screen);
      // Ends the test.
      loop.Quit();
    }));

    loop.Run();
  }
}

// Disabled test: https://crbug.com/1062668
TEST_F(IdleManagerTest, DISABLED_LockingScreenAfterIdle) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(Return(base::TimeDelta::FromSeconds(0)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(false));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting(
            [&](IdleManagerError error, IdleStatePtr state) {
              EXPECT_EQ(IdleManagerError::kSuccess, error);
              EXPECT_EQ(UserIdleState::kActive, state->user);
              EXPECT_EQ(ScreenIdleState::kUnlocked, state->screen);
              loop.Quit();
            }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going idle, but with the screen still unlocked.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(false));

    // Expects Update to be notified about the change to idle.
    EXPECT_CALL(monitor, Update(_)).WillOnce(Invoke([&](IdleStatePtr state) {
      EXPECT_EQ(UserIdleState::kIdle, state->user);
      EXPECT_EQ(ScreenIdleState::kUnlocked, state->screen);
      loop.Quit();
    }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates the screeng getting locked by the system after the user goes
    // idle (e.g. screensaver kicks in first, throwing idleness, then getting
    // locked).
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(true));

    // Expects Update to be notified about the change to locked.
    EXPECT_CALL(monitor, Update(_)).WillOnce(Invoke([&](IdleStatePtr state) {
      EXPECT_EQ(ScreenIdleState::kLocked, state->screen);
      EXPECT_EQ(UserIdleState::kIdle, state->user);
      // Ends the test.
      loop.Quit();
    }));
    loop.Run();
  }
}

TEST_F(IdleManagerTest, RemoveMonitorStopsPolling) {
  // Simulates the renderer disconnecting (e.g. on page reload) and verifies
  // that the polling stops for the idle detection.

  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting(
            [&](IdleManagerError error, IdleStatePtr state) { loop.Quit(); }));

    loop.Run();
  }

  EXPECT_TRUE(impl->IsPollingForTest());

  {
    base::RunLoop loop;

    // Simulates the renderer disconnecting.
    monitor_receiver.reset();

    // Wait for the IdleManager to observe the pipe close.
    loop.RunUntilIdle();
  }

  EXPECT_FALSE(impl->IsPollingForTest());
}

TEST_F(IdleManagerTest, Threshold) {
  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  base::RunLoop loop;

  // Initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime())
      .WillRepeatedly(Return(base::TimeDelta::FromSeconds(91)));
  EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillRepeatedly(Return(false));

  service_remote->AddMonitor(
      base::TimeDelta::FromSeconds(90),
      monitor_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting(
          [&](IdleManagerError error, IdleStatePtr state) {
            EXPECT_EQ(IdleManagerError::kSuccess, error);
            EXPECT_EQ(UserIdleState::kIdle, state->user);
            loop.Quit();
          }));

  loop.Run();
}

TEST_F(IdleManagerTest, InvalidThreshold) {
  SetPermissionStatus(url(), blink::mojom::PermissionStatus::GRANTED);
  mojo::test::BadMessageObserver bad_message_observer;
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  // Should not start initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime()).Times(0);
  EXPECT_CALL(*mock, CheckIdleStateIsLocked()).Times(0);

  service_remote->AddMonitor(base::TimeDelta::FromSeconds(50),
                             monitor_receiver.BindNewPipeAndPassRemote(),
                             base::NullCallback());
  EXPECT_EQ("Minimum threshold is 60 seconds.",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(IdleManagerTest, NotificationPermissionDisabled) {
  SetPermissionStatus(url(), blink::mojom::PermissionStatus::DENIED);
  auto* impl = GetIdleManager();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver(), origin());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  // Should not start initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime()).Times(0);
  EXPECT_CALL(*mock, CheckIdleStateIsLocked()).Times(0);

  base::RunLoop loop;

  service_remote->AddMonitor(
      base::TimeDelta::FromSeconds(90),
      monitor_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](IdleManagerError error,
                                     IdleStatePtr state) {
        EXPECT_EQ(blink::mojom::IdleManagerError::kPermissionDisabled, error);
        EXPECT_FALSE(state);
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace content
