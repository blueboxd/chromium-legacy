// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <string>
#include <utility>

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"

namespace ash {

namespace {

constexpr base::TimeDelta kDefaultTokenExpirationDelay =
    base::TimeDelta::FromHours(1);

}  // namespace

using AmbientControllerTest = AmbientAshTestBase;

TEST_F(AmbientControllerTest, ShowAmbientContainerViewOnLockScreen) {
  EXPECT_FALSE(ambient_controller()->is_showing());

  LockScreen();
  EXPECT_TRUE(ambient_controller()->is_showing());
}

TEST_F(AmbientControllerTest, ShouldRequestAccessTokenWhenLockingScreen) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Should close ambient widget already when unlocking screen.
  ambient_controller()->Toggle();
  UnlockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());
}

TEST_F(AmbientControllerTest, ShouldReturnCachedAccessToken) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Another token request will return cached token.
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::RunLoop run_loop;
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_EQ(access_token_fetched, access_token);

        std::move(closure).Run();
        run_loop.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop.Run();
}

TEST_F(AmbientControllerTest, ShouldRefreshAccessTokenAfterFailure) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Token request automatically retry.
  // The failure delay has jitter so fast forward a bit more, but before
  // the returned token would expire again.
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay / 2);
  EXPECT_TRUE(IsAccessTokenRequestPending());
}

}  // namespace ash
