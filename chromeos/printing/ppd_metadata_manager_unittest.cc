// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chromeos/printing/ppd_metadata_manager.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/clock.h"
#include "chromeos/printing/fake_printer_config_cache.h"
#include "chromeos/printing/ppd_metadata_matchers.h"
#include "chromeos/printing/ppd_metadata_parser.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_config_cache.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

// Default browser locale used to construct PpdMetadataManager instances
// in the test fixture. Arbitrarily chosen. Changeable by calling
// PpdMetadataManagerTest::NewManagerWithLocale().
constexpr base::StringPiece kBrowserLocaleForTesting = "en-US";

// Arbitrarily chosen TimeDelta used in test cases that are not
// time-senstive.
constexpr base::TimeDelta kArbitraryTimeDelta =
    base::TimeDelta::FromSeconds(30LL);

// Arbitrarily malformed JSON used to exercise code paths in which
// parsing fails.
constexpr base::StringPiece kInvalidJson = "blah blah invalid JSON";

// Caller may bind a default-constructed base::RepeatingClosure to
// any Catch*() method, indicating that they don't want anything run.
class PpdMetadataManagerTest : public ::testing::Test {
 public:
  // Callback method appropriate for passing to
  // PpdMetadataManager::GetLocale().
  void CatchGetLocale(base::RepeatingClosure quit_closure, bool succeeded) {
    results_.get_locale_succeeded = succeeded;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  // Callback method appropriate for passing to
  // PpdMetadataManager::GetManufacturers().
  void CatchGetManufacturers(base::RepeatingClosure quit_closure,
                             PpdProvider::CallbackResultCode code,
                             const std::vector<std::string>& manufacturers) {
    results_.get_manufacturers_code = code;
    results_.manufacturers = manufacturers;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  // Callback method appropriate for passing to
  // PpdMetadataManager::GetPrinters().
  void CatchGetPrinters(base::RepeatingClosure quit_closure,
                        bool succeeded,
                        const ParsedPrinters& printers) {
    results_.get_printers_succeeded = succeeded;
    results_.printers = printers;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  // Callback method appropriate for passing to
  // PpdMetadataManager::SplitMakeAndModel().
  void CatchSplitMakeAndModel(base::RepeatingClosure quit_closure,
                              PpdProvider::CallbackResultCode code,
                              const std::string& make,
                              const std::string& model) {
    results_.split_make_and_model_code = code;
    results_.split_make = make;
    results_.split_model = model;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

 protected:
  // Convenience container that organizes all callback results.
  struct CallbackLandingArea {
    CallbackLandingArea()
        : get_locale_succeeded(false), get_printers_succeeded(false) {}
    ~CallbackLandingArea() = default;

    // Landing area for PpdMetadataManager::GetLocale().
    bool get_locale_succeeded;

    // Landing area for PpdMetadataManager::GetManufacturers().
    PpdProvider::CallbackResultCode get_manufacturers_code;
    std::vector<std::string> manufacturers;

    // Landing area for PpdMetadataManager::GetPrinters().
    bool get_printers_succeeded;
    ParsedPrinters printers;

    // Landing area for PpdMetadataManager::SplitMakeAndModel().
    PpdProvider::CallbackResultCode split_make_and_model_code;
    std::string split_make;
    std::string split_model;
  };

  PpdMetadataManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        manager_(PpdMetadataManager::Create(
            kBrowserLocaleForTesting,
            &clock_,
            std::make_unique<FakePrinterConfigCache>())) {}

  // Borrows and returns a pointer to the config cache owned by the
  // |manager_|.
  //
  // Useful for adjusting availability of (fake) network resources.
  FakePrinterConfigCache* GetFakeCache() {
    return reinterpret_cast<FakePrinterConfigCache*>(
        manager_->GetPrinterConfigCacheForTesting());
  }

  // Recreates |manager_| with a new |browser_locale|.
  //
  // Useful for testing the manager's ability to parse and select a
  // proper metadata locale.
  void NewManagerWithLocale(base::StringPiece browser_locale) {
    manager_.reset();
    manager_ = PpdMetadataManager::Create(
        browser_locale, &clock_, std::make_unique<FakePrinterConfigCache>());
  }

  // Holder for all callback results.
  CallbackLandingArea results_;

  // Environment for task schedulers.
  base::test::TaskEnvironment task_environment_;

  // Controlled clock that dispenses times of Fetch().
  base::SimpleTestClock clock_;

  // Class under test.
  std::unique_ptr<PpdMetadataManager> manager_;
};

// Verifies that the manager can fetch and parse the best-fit
// locale from the Chrome OS Printing serving root.
//
// This test is done against the default browser locale used
// throughout this suite, "en-US."
TEST_F(PpdMetadataManagerTest, CanGetLocale) {
  // Known interaction: the manager will fetch the locales metadata.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/locales.json", R"({ "locales": [ "de", "en", "es" ] })");

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetLocale,
                                 base::Unretained(this), loop.QuitClosure());
  auto call =
      base::BindOnce(&PpdMetadataManager::GetLocale,
                     base::Unretained(manager_.get()), std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_TRUE(results_.get_locale_succeeded);
  EXPECT_THAT(manager_->ExposeMetadataLocaleForTesting(), StrEq("en"));
}

// Verifies that the manager defaults to the English ("en") locale
// when it can find no closer fit for the browser locale.
TEST_F(PpdMetadataManagerTest, DefaultsToEnglishLocale) {
  // Sets an arbitrarily chosen locale quite distant from what the
  // fake serving root will have available.
  NewManagerWithLocale("ja-JP");

  // Known interaction: the manager will fetch the locales metadata.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/locales.json",
      R"({ "locales": [ "de", "en", "es", "wo" ] })");

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetLocale,
                                 base::Unretained(this), loop.QuitClosure());
  auto call =
      base::BindOnce(&PpdMetadataManager::GetLocale,
                     base::Unretained(manager_.get()), std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_TRUE(results_.get_locale_succeeded);
  EXPECT_THAT(manager_->ExposeMetadataLocaleForTesting(), StrEq("en"));
}

// Given that the browser locale is not "en-US," verifies that the
// manager can select a best-fit locale when one is available.
TEST_F(PpdMetadataManagerTest, CanSelectNonEnglishCloseFitLocale) {
  // It's not "en-US" and is close to advertised metadata locale "es."
  NewManagerWithLocale("es-MX");

  // Known interaction: the manager will fetch the locales metadata.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/locales.json",
      R"({ "locales": [ "de", "en", "es", "wo" ] })");

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetLocale,
                                 base::Unretained(this), loop.QuitClosure());
  auto call =
      base::BindOnce(&PpdMetadataManager::GetLocale,
                     base::Unretained(manager_.get()), std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_TRUE(results_.get_locale_succeeded);
  EXPECT_THAT(manager_->ExposeMetadataLocaleForTesting(), StrEq("es"));
}

// Verifies that the manager fails the GetLocaleCallback
// *  if it finds no close fit for the browser locale and
// *  if the serving root does not advertise availability of
//    English-localized metadata.
TEST_F(PpdMetadataManagerTest, FailsToFindAnyCloseFitLocale) {
  // Sets an arbitrarily chosen locale quite distant from what the
  // fake serving root will have available.
  NewManagerWithLocale("ja-JP");

  // Known interaction: the manager will fetch the locales metadata.
  //
  // Note that we are canning well-formed JSON.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/locales.json", R"({ "locales": [ "de", "es", "wo" ] })");

  // Jams the result to the opposite of what's expected.
  results_.get_locale_succeeded = true;

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetLocale,
                                 base::Unretained(this), loop.QuitClosure());
  auto call =
      base::BindOnce(&PpdMetadataManager::GetLocale,
                     base::Unretained(manager_.get()), std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_FALSE(results_.get_locale_succeeded);
  EXPECT_THAT(manager_->ExposeMetadataLocaleForTesting(), StrEq(""));
}

// Verifies that the manager fails the GetLocaleCallback if it fails to
// fetch the locales metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetLocaleOnFetchFailure) {
  // This test deliberately doesn't can any response from the
  // FakePrinterConfigCache. We want to see what happens when the
  // manager fails to fetch the necessary networked resource.
  //
  // We do need some way to tell that GetLocale() failed, so we start
  // by jamming it to the opposite of the expected value.
  results_.get_locale_succeeded = true;

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetLocale,
                                 base::Unretained(this), loop.QuitClosure());
  auto call =
      base::BindOnce(&PpdMetadataManager::GetLocale,
                     base::Unretained(manager_.get()), std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_FALSE(results_.get_locale_succeeded);
  EXPECT_THAT(manager_->ExposeMetadataLocaleForTesting(), StrEq(""));
}

// Verifies that the manager fails the GetLocaleCallback if it fails to
// parse the locales metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetLocaleOnParseFailure) {
  // Known interaction: the manager will fetch the locales metadata.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/locales.json",
                                             kInvalidJson);

  // We've canned an unparsable response for the manager.
  // To observe that GetLocale() fails, we jam the result to the
  // opposite of the expected value.
  results_.get_locale_succeeded = true;

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetLocale,
                                 base::Unretained(this), loop.QuitClosure());
  auto call =
      base::BindOnce(&PpdMetadataManager::GetLocale,
                     base::Unretained(manager_.get()), std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_FALSE(results_.get_locale_succeeded);
  EXPECT_THAT(manager_->ExposeMetadataLocaleForTesting(), StrEq(""));
}

// Verifies that the manager can fetch, parse, and return a list of
// manufacturers from the Chrome OS Printing serving root.
TEST_F(PpdMetadataManagerTest, CanGetManufacturers) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Known interaction: the manager will fetch manufacturers metadata
  // localized in English ("en").
  //
  // In real life, the values of the filesMap dictionary have a
  // hyphenated locale suffix attached; this is not something the
  // manager actually cares about and is not something used directly
  // in this test case.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/manufacturers-en.json",
      R"({ "filesMap": {
        "It": "Never_Ends-en.json",
        "You Are": "Always-en.json",
        "Playing": "Yellow_Car-en.json"
      } })");

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetManufacturers,
                             base::Unretained(manager_.get()),
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::SUCCESS);

  // PpdProvider::ResolveManufacturersCallback specifies that the list
  // shall be sorted.
  EXPECT_THAT(results_.manufacturers,
              ElementsAre(StrEq("It"), StrEq("Playing"), StrEq("You Are")));
}

// Verifies that the manager fails the ResolveManufacturersCallback
// when it fails to fetch the manufacturers metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetManufacturersOnFetchFailure) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Known interaction: the manager will fetch manufacturers metadata
  // localized in English ("en"). In this test case, we do _not_
  // populate the fake config cache with the appropriate metadata,
  // causing the fetch to fail.

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetManufacturers,
                             base::Unretained(manager_.get()),
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::SERVER_ERROR);
}

// Verifies that the manager fails the ResolveManufacturersCallback
// when it fails to parse the manufacturers metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetManufacturersOnParseFailure) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Known interaction: the manager will fetch manufacturers metadata
  // localized in English ("en").
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/manufacturers-en.json", kInvalidJson);

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetManufacturers,
                             base::Unretained(manager_.get()),
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::INTERNAL_ERROR);
}

// Verifies that the manager can fetch, parse, and return a map of
// printers metadata from the Chrome OS Printing serving root.
TEST_F(PpdMetadataManagerTest, CanGetPrinters) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Bypasses prerequisite call to PpdMetadataManager::GetManufacturers().
  ASSERT_TRUE(manager_->SetManufacturersForTesting(R"(
  {
    "filesMap": {
      "Manufacturer A": "Manufacturer_A-en.json",
      "Manufacturer B": "Manufacturer_B-en.json"
    }
  }
  )"));

  // Known interaction: the manager will fetch printers metadata named
  // by the manufacturers map above.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/Manufacturer_A-en.json", R"(
      {
        "printers": [ {
          "emm": "some emm a",
          "name": "Some Printer A"
        }, {
          "emm": "some emm b",
          "name": "Some Printer B"
        } ]
      }
  )");

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetPrinters,
                             base::Unretained(manager_.get()), "Manufacturer A",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_TRUE(results_.get_printers_succeeded);
  EXPECT_THAT(
      results_.printers,
      UnorderedElementsAre(ParsedPrinterLike("Some Printer A", "some emm a"),
                           ParsedPrinterLike("Some Printer B", "some emm b")));
}

// Verifies that the manager fails the GetPrintersCallback when it fails
// to fetch the printers metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetPrintersOnFetchFailure) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Bypasses prerequisite call to PpdMetadataManager::GetManufacturers().
  ASSERT_TRUE(manager_->SetManufacturersForTesting(R"(
  {
    "filesMap": {
      "Manufacturer A": "Manufacturer_A-en.json",
      "Manufacturer B": "Manufacturer_B-en.json"
    }
  }
  )"));

  // This test is set up like the CanGetPrinters test case above, but we
  // elect _not_ to provide a response for any printers metadata,
  // causing the fetch to fail.
  //
  // We set the result value to the opposite of what's expected to
  // observe the change.
  results_.get_printers_succeeded = true;

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetPrinters,
                             base::Unretained(manager_.get()), "Manufacturer A",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  EXPECT_FALSE(results_.get_printers_succeeded);
}

// Verifies that the manager fails the GetPrintersCallback when it fails
// to parse the printers metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetPrintersOnParseFailure) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Bypasses prerequisite call to PpdMetadataManager::GetManufacturers().
  ASSERT_TRUE(manager_->SetManufacturersForTesting(R"(
  {
    "filesMap": {
      "Manufacturer A": "Manufacturer_A-en.json",
      "Manufacturer B": "Manufacturer_B-en.json"
    }
  }
  )"));

  // This test is set up like the CanGetPrinters test case above, but we
  // elect to provide a malformed JSON response for the printers
  // metadata, which will cause the manager to fail parsing.
  //
  // Known interaction: the manager will fetch the printers metadata
  // named by the map above.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/Manufacturer_A-en.json", kInvalidJson);

  // We set the result value to the opposite of what's expected to
  // observe the change.
  results_.get_printers_succeeded = true;

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetPrinters,
                             base::Unretained(manager_.get()), "Manufacturer A",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  EXPECT_FALSE(results_.get_printers_succeeded);
}

// Verifies that the manager can split an effective-make-and-model
// string into its constituent parts (make and model).
TEST_F(PpdMetadataManagerTest, CanSplitMakeAndModel) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Known interaction: asking the manager to split the string
  // "Hello there!" will cause it to fetch the reverse index metadata
  // with shard number 2.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/reverse_index-en-02.json", R"(
      {
        "reverseIndex": {
          "Hello there!": {
            "manufacturer": "General",
            "model": "Kenobi"
          }
        }
      }
  )");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::SplitMakeAndModel,
                             base::Unretained(manager_.get()), "Hello there!",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::SUCCESS);
  EXPECT_THAT(results_.split_make, StrEq("General"));
  EXPECT_THAT(results_.split_model, StrEq("Kenobi"));
}

// Verifies that the manager fails the ReverseLookupCallback when it
// fails to fetch the necessary metadata from the Chrome OS Printing
// serving root.
TEST_F(PpdMetadataManagerTest, FailsToSplitMakeAndModelOnFetchFailure) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Known interaction: asking the manager to split the string
  // "Hello there!" will cause it to fetch the reverse index metadata
  // with shard number 2.
  //
  // We elect _not_ to fake a value for this s.t. the fetch will fail.

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::SplitMakeAndModel,
                             base::Unretained(manager_.get()), "Hello there!",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  EXPECT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::SERVER_ERROR);
}

// Verifies that the manager fails the ReverseLookupCallback when it
// fails to parse the necessary metadata from the Chrome OS Printing
// serving root.
TEST_F(PpdMetadataManagerTest, FailsToSplitMakeAndModelOnParseFailure) {
  // Bypasses mandatory call to PpdMetadataManager::GetLocale().
  manager_->SetLocaleForTesting("en");

  // Known interaction: asking the manager to split the string
  // "Hello there!" will cause it to fetch the reverse index metadata
  // with shard number 2.
  //
  // We fake a fetch value that is invalid JSON s.t. the manager
  // will fail to parse it.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/reverse_index-en-02.json", kInvalidJson);

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::SplitMakeAndModel,
                             base::Unretained(manager_.get()), "Hello there!",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(call));
  loop.Run();

  ASSERT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::INTERNAL_ERROR);
}

}  // namespace
}  // namespace chromeos
