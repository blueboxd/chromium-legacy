// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/nigori/cryptographer.h"

namespace {

using bookmarks_helper::AddURL;
using bookmarks_helper::CreateBookmarkServerEntity;
using encryption_helper::CreateCustomPassphraseNigori;
using encryption_helper::GetEncryptedBookmarkEntitySpecifics;
using encryption_helper::GetServerNigori;
using encryption_helper::InitCustomPassphraseCryptographerFromNigori;
using encryption_helper::SetNigoriInFakeServer;
using fake_server::FakeServer;
using sync_pb::EncryptedData;
using sync_pb::NigoriSpecifics;
using sync_pb::SyncEntity;
using syncer::Cryptographer;
using syncer::KeyDerivationParams;
using syncer::KeyParams;
using syncer::LoopbackServerEntity;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::PassphraseType;
using syncer::ProtoPassphraseInt32ToEnum;
using syncer::SyncService;

class DatatypeCommitCountingFakeServerObserver : public FakeServer::Observer {
 public:
  explicit DatatypeCommitCountingFakeServerObserver(FakeServer* fake_server)
      : fake_server_(fake_server) {
    fake_server->AddObserver(this);
  }

  void OnCommit(const std::string& committer_id,
                ModelTypeSet committed_model_types) override {
    for (ModelType type : committed_model_types) {
      ++datatype_commit_counts_[type];
    }
  }

  int GetCommitCountForDatatype(ModelType type) {
    return datatype_commit_counts_[type];
  }

  ~DatatypeCommitCountingFakeServerObserver() override {
    fake_server_->RemoveObserver(this);
  }

 private:
  FakeServer* fake_server_;
  std::map<syncer::ModelType, int> datatype_commit_counts_;
};

// These tests use a gray-box testing approach to verify that the data committed
// to the server is encrypted properly, and that properly-encrypted data from
// the server is successfully decrypted by the client. They also verify that the
// key derivation methods are set, read and handled properly. They do not,
// however, directly ensure that two clients syncing through the same account
// will be able to access each others' data in the presence of a custom
// passphrase. For this, a separate two-client test is be used.
class SingleClientCustomPassphraseSyncTest : public SyncTest {
 public:
  SingleClientCustomPassphraseSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientCustomPassphraseSyncTest() override {}

  // Waits until the given set of bookmarks appears on the server, encrypted
  // according to the server-side Nigori and with the given passphrase.
  bool WaitForEncryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks,
      const std::string& passphrase) {
    auto cryptographer = CreateCryptographerFromServerNigori(passphrase);
    return ServerBookmarksEqualityChecker(GetSyncService(), GetFakeServer(),
                                          expected_bookmarks,
                                          cryptographer.get())
        .Wait();
  }

  // Waits until the given set of bookmarks appears on the server, encrypted
  // with the precise KeyParams given.
  bool WaitForEncryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks,
      const KeyParams& key_params) {
    auto cryptographer = CreateCryptographerWithKeyParams(key_params);
    return ServerBookmarksEqualityChecker(GetSyncService(), GetFakeServer(),
                                          expected_bookmarks,
                                          cryptographer.get())
        .Wait();
  }

  bool WaitForUnencryptedServerBookmarks(
      const std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark>&
          expected_bookmarks) {
    return ServerBookmarksEqualityChecker(GetSyncService(), GetFakeServer(),
                                          expected_bookmarks,
                                          /*cryptographer=*/nullptr)
        .Wait();
  }

  bool WaitForNigori(PassphraseType expected_passphrase_type) {
    return ServerNigoriChecker(GetSyncService(), GetFakeServer(),
                               expected_passphrase_type)
        .Wait();
  }

  bool WaitForPassphraseRequiredState(bool desired_state) {
    return PassphraseRequiredStateChecker(GetSyncService(), desired_state)
        .Wait();
  }

  bool WaitForClientBookmarkWithTitle(std::string title) {
    return BookmarksTitleChecker(/*profile_index=*/0, title,
                                 /*expected_count=*/1)
        .Wait();
  }

  syncer::ProfileSyncService* GetSyncService() {
    return SyncTest::GetSyncService(0);
  }

  // When the cryptographer is initialized with a passphrase, it uses the key
  // derivation method and other parameters from the server-side Nigori. Thus,
  // checking that the server-side Nigori contains the desired key derivation
  // method and checking that the server-side encrypted bookmarks can be
  // decrypted using a cryptographer initialized with this function is
  // sufficient to determine that a given key derivation method is being
  // correctly used for encryption.
  std::unique_ptr<Cryptographer> CreateCryptographerFromServerNigori(
      const std::string& passphrase) {
    NigoriSpecifics nigori;
    EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
    EXPECT_EQ(ProtoPassphraseInt32ToEnum(nigori.passphrase_type()),
              PassphraseType::CUSTOM_PASSPHRASE);
    auto cryptographer = std::make_unique<Cryptographer>();
    InitCustomPassphraseCryptographerFromNigori(nigori, cryptographer.get(),
                                                passphrase);
    return cryptographer;
  }

  // A cryptographer initialized with the given KeyParams has not "seen" the
  // server-side Nigori, and so any data decryptable by such a cryptographer
  // does not depend on external info.
  std::unique_ptr<Cryptographer> CreateCryptographerWithKeyParams(
      const KeyParams& key_params) {
    auto cryptographer = std::make_unique<Cryptographer>();
    cryptographer->AddKey(key_params);
    return cryptographer;
  }

  void InjectEncryptedServerBookmark(const std::string& title,
                                     const GURL& url,
                                     const KeyParams& key_params) {
    std::unique_ptr<LoopbackServerEntity> server_entity =
        CreateBookmarkServerEntity(title, url);
    server_entity->SetSpecifics(GetEncryptedBookmarkEntitySpecifics(
        server_entity->GetSpecifics().bookmark(), key_params));
    GetFakeServer()->InjectEntity(std::move(server_entity));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientCustomPassphraseSyncTest);
};

class SingleClientCustomPassphraseSyncTestWithUssTests
    : public SingleClientCustomPassphraseSyncTest,
      public testing::WithParamInterface<bool> {
 public:
  SingleClientCustomPassphraseSyncTestWithUssTests() {
    if (GetParam()) {
      // USS Nigori requires USS implementations to be enabled for all
      // datatypes.
      override_features_.InitWithFeatures(
          /*enabled_features=*/{switches::kSyncUSSBookmarks,
                                switches::kSyncUSSPasswords,
                                switches::kSyncUSSAutofillWalletMetadata,
                                switches::kSyncUSSNigori},
          /*disabled_features=*/{});
    } else {
      // We test Directory Nigori with default values of USS feature flags of
      // other datatypes.
      override_features_.InitAndDisableFeature(switches::kSyncUSSNigori);
    }
  }
  ~SingleClientCustomPassphraseSyncTestWithUssTests() override = default;

 private:
  base::test::ScopedFeatureList override_features_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientCustomPassphraseSyncTestWithUssTests);
};

class SingleClientCustomPassphraseForceDisableScryptSyncTest
    : public SingleClientCustomPassphraseSyncTest {
 public:
  SingleClientCustomPassphraseForceDisableScryptSyncTest()
      : features_(/*force_disabled=*/true, /*use_for_new_passphrases=*/false) {}

 private:
  ScopedScryptFeatureToggler features_;
};

class SingleClientCustomPassphraseDoNotUseScryptSyncTest
    : public SingleClientCustomPassphraseSyncTestWithUssTests {
 public:
  SingleClientCustomPassphraseDoNotUseScryptSyncTest()
      : features_(/*force_disabled=*/false, /*use_for_new_passphrases=*/false) {
  }

 private:
  ScopedScryptFeatureToggler features_;
};

class SingleClientCustomPassphraseUseScryptSyncTest
    : public SingleClientCustomPassphraseSyncTest {
 public:
  SingleClientCustomPassphraseUseScryptSyncTest()
      : features_(/*force_disabled=*/false, /*use_for_new_passphrases=*/true) {}

 private:
  ScopedScryptFeatureToggler features_;
};

IN_PROC_BROWSER_TEST_P(SingleClientCustomPassphraseSyncTestWithUssTests,
                       CommitsEncryptedData) {
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(
      AddURL(/*profile=*/0, "Hello world", GURL("https://google.com/")));
  ASSERT_TRUE(
      AddURL(/*profile=*/0, "Bookmark #2", GURL("https://example.com/")));
  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));

  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{"Hello world", GURL("https://google.com/")},
       {"Bookmark #2", GURL("https://example.com/")}},
      /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_P(SingleClientCustomPassphraseSyncTestWithUssTests,
                       CanDecryptPbkdf2KeyEncryptedData) {
  KeyParams key_params = {KeyDerivationParams::CreateForPbkdf2(), "hunter2"};
  InjectEncryptedServerBookmark("PBKDF2-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                key_params);
  SetNigoriInFakeServer(GetFakeServer(),
                        CreateCustomPassphraseNigori(key_params));
  SetDecryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/false));

  EXPECT_TRUE(WaitForClientBookmarkWithTitle("PBKDF2-encrypted bookmark"));
}

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientCustomPassphraseSyncTestWithUssTests,
                         testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(SingleClientCustomPassphraseDoNotUseScryptSyncTest,
                       CommitsEncryptedDataUsingPbkdf2WhenScryptDisabled) {
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddURL(/*profile=*/0, "PBKDF2 encrypted",
                     GURL("https://google.com/pbkdf2-encrypted")));

  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));
  NigoriSpecifics nigori;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
  EXPECT_EQ(nigori.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::PBKDF2_HMAC_SHA1_1003);
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{"PBKDF2 encrypted", GURL("https://google.com/pbkdf2-encrypted")}},
      /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseUseScryptSyncTest,
                       CommitsEncryptedDataUsingScryptWhenScryptEnabled) {
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AddURL(/*profile=*/0, "scrypt encrypted",
                     GURL("https://google.com/scrypt-encrypted")));

  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));
  NigoriSpecifics nigori;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &nigori));
  EXPECT_EQ(nigori.custom_passphrase_key_derivation_method(),
            sync_pb::NigoriSpecifics::SCRYPT_8192_8_11);
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{"scrypt encrypted", GURL("https://google.com/scrypt-encrypted")}},
      /*passphrase=*/"hunter2"));
}

IN_PROC_BROWSER_TEST_P(SingleClientCustomPassphraseDoNotUseScryptSyncTest,
                       CanDecryptScryptKeyEncryptedDataWhenScryptNotDisabled) {
  KeyParams key_params = {
      KeyDerivationParams::CreateForScrypt("someConstantSalt"), "hunter2"};
  InjectEncryptedServerBookmark("scypt-encrypted bookmark",
                                GURL("http://example.com/doesnt-matter"),
                                key_params);
  SetNigoriInFakeServer(GetFakeServer(),
                        CreateCustomPassphraseNigori(key_params));
  SetDecryptionPassphraseForClient(/*index=*/0, "hunter2");

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/false));

  EXPECT_TRUE(WaitForClientBookmarkWithTitle("scypt-encrypted bookmark"));
}

// See crbug.com/915219 about THREAD_SANITIZER. This data race is hard to avoid
// as overriding g_feature_list after it has been used is needed for this test.
#if defined(THREAD_SANITIZER)
#define MAYBE_CannotDecryptScryptKeyEncryptedDataWhenScryptDisabled \
  DISABLED_CannotDecryptScryptKeyEncryptedDataWhenScryptDisabled
#else
#define MAYBE_CannotDecryptScryptKeyEncryptedDataWhenScryptDisabled \
  CannotDecryptScryptKeyEncryptedDataWhenScryptDisabled
#endif
IN_PROC_BROWSER_TEST_F(
    SingleClientCustomPassphraseForceDisableScryptSyncTest,
    MAYBE_CannotDecryptScryptKeyEncryptedDataWhenScryptDisabled) {
  sync_pb::NigoriSpecifics nigori;
  {
    // Creating a Nigori and injecting an encrypted bookmark both require key
    // derivation using scrypt, so temporarily enable it. This overrides the
    // feature toggle in the test fixture, which will be restored after this
    // block.
    ScopedScryptFeatureToggler toggler(/*force_disabled=*/false,
                                       /*use_for_new_passphrases_=*/false);
    KeyParams key_params = {
        KeyDerivationParams::CreateForScrypt("someConstantSalt"), "hunter2"};
    nigori = CreateCustomPassphraseNigori(key_params);
    InjectEncryptedServerBookmark("scypt-encrypted bookmark",
                                  GURL("http://example.com/doesnt-matter"),
                                  key_params);
  }
  SetNigoriInFakeServer(GetFakeServer(), nigori);
  SetDecryptionPassphraseForClient(/*index=*/0, "hunter2");

  SetupSyncNoWaitingForCompletion();

  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/true));
}

IN_PROC_BROWSER_TEST_P(SingleClientCustomPassphraseDoNotUseScryptSyncTest,
                       DoesNotLeakUnencryptedData) {
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  DatatypeCommitCountingFakeServerObserver observer(GetFakeServer());
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AddURL(/*profile=*/0, "Should be encrypted",
                     GURL("https://google.com/encrypted")));

  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));
  // If WaitForEncryptedServerBookmarks() succeeds, that means that a
  // cryptographer initialized with only the key params was able to decrypt the
  // data, so the data must be encrypted using a passphrase-derived key (and not
  // e.g. a keystore key), because that cryptographer has never seen the
  // server-side Nigori. Furthermore, if a bookmark commit has happened only
  // once, we are certain that no bookmarks other than those we've verified to
  // be encrypted have been committed.
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      {{"Should be encrypted", GURL("https://google.com/encrypted")}},
      {KeyDerivationParams::CreateForPbkdf2(), "hunter2"}));
  // Initial bookmarks sync would actually create and commit the permanent
  // bookmark folders. Therefore, should be 2 commits by now.
  EXPECT_EQ(observer.GetCommitCountForDatatype(syncer::BOOKMARKS), 2);
}

IN_PROC_BROWSER_TEST_P(SingleClientCustomPassphraseDoNotUseScryptSyncTest,
                       ReencryptsDataWhenPassphraseIsSet) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForNigori(PassphraseType::KEYSTORE_PASSPHRASE));
  ASSERT_TRUE(AddURL(/*profile=*/0, "Re-encryption is great",
                     GURL("https://google.com/re-encrypted")));
  std::vector<ServerBookmarksEqualityChecker::ExpectedBookmark> expected = {
      {"Re-encryption is great", GURL("https://google.com/re-encrypted")}};
  ASSERT_TRUE(WaitForUnencryptedServerBookmarks(expected));

  GetSyncService()->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));

  // If WaitForEncryptedServerBookmarks() succeeds, that means that a
  // cryptographer initialized with only the key params was able to decrypt the
  // data, so the data must be encrypted using a passphrase-derived key (and not
  // e.g. the previous keystore key which was stored in the Nigori keybag),
  // because that cryptographer has never seen the server-side Nigori.
  EXPECT_TRUE(WaitForEncryptedServerBookmarks(
      expected, {KeyDerivationParams::CreateForPbkdf2(), "hunter2"}));
}

// TODO(https://crbug.com/952074): re-enable once flakiness is addressed.
#if defined(THREAD_SANITIZER)
#define MAYBE_PRE_ShouldLoadUSSCustomPassphraseInDirectoryMode \
  DISABLED_PRE_ShouldLoadUSSCustomPassphraseInDirectoryMode
#else
#define MAYBE_PRE_ShouldLoadUSSCustomPassphraseInDirectoryMode \
  PRE_ShouldLoadUSSCustomPassphraseInDirectoryMode
#endif

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       PRE_ShouldLoadUSSCustomPassphraseInDirectoryMode) {
  base::test::ScopedFeatureList override_features;
  // TODO(crbug.com/922900): Don't disable scrypt derivation and use it as key
  // derivation method in ShouldLoadUSSCustomPassphraseInDirectoryMode, once
  // USS implementation support it for new passphrases.
  override_features.InitWithFeatures(
      /*enabled_features=*/{switches::kSyncUSSBookmarks,
                            switches::kSyncUSSPasswords,
                            switches::kSyncUSSAutofillWalletMetadata,
                            switches::kSyncUSSNigori},
      /*disabled_features=*/{switches::kSyncUseScryptForNewCustomPassphrases});
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(WaitForNigori(PassphraseType::KEYSTORE_PASSPHRASE));
  GetSyncService()->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(WaitForNigori(PassphraseType::CUSTOM_PASSPHRASE));
}

// TODO(https://crbug.com/952074): re-enable once flakiness is addressed.
#if defined(THREAD_SANITIZER)
#define MAYBE_ShouldLoadUSSCustomPassphraseInDirectoryMode \
  DISABLED_ShouldLoadUSSCustomPassphraseInDirectoryMode
#else
#define MAYBE_ShouldLoadUSSCustomPassphraseInDirectoryMode \
  ShouldLoadUSSCustomPassphraseInDirectoryMode
#endif

IN_PROC_BROWSER_TEST_F(SingleClientCustomPassphraseSyncTest,
                       ShouldLoadUSSCustomPassphraseInDirectoryMode) {
  // We should be able to decrypt bookmarks with passphrase, which was set when
  // kSyncUSSNigori was enabled, without providing it again once kSyncUSSNigori
  // is disabled.
  base::test::ScopedFeatureList override_features;
  override_features.InitAndDisableFeature(switches::kSyncUSSNigori);
  const KeyParams key_params = {KeyDerivationParams::CreateForPbkdf2(),
                                "hunter2"};
  InjectEncryptedServerBookmark(
      "some bookmark", GURL("http://example.com/doesnt-matter"), key_params);
  ASSERT_TRUE(SetupClients());

  EXPECT_TRUE(WaitForPassphraseRequiredState(/*desired_state=*/false));
  EXPECT_TRUE(WaitForClientBookmarkWithTitle("some bookmark"));
}

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientCustomPassphraseDoNotUseScryptSyncTest,
                         testing::Values(false, true));

}  // namespace
