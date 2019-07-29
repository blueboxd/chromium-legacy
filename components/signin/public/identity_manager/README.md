The core public API surfaces for interacting with Google identity. Header files
in this directory are allowed to depend only on the following other parts of the
signin component:

//components/signin/public/base
//components/signin/public/webdata

Implementation files in this directory, however, are additionally allowed to
depend on //components/signin/internal/identity_manager.

A quick guide through the core concepts:

- "Account" always refers to a Gaia account.
- "Primary account" in IdentityManager refers to what is called the
  "authenticated account" in PrimaryAccountManager, i.e., the account that has
  been blessed for sync by the user.
- "Unconsented primary account" is intuitively the browsing identity of the user
  that we display to the user; the user may or may not have blessed this account
  for sync. In particular, whenever a primary account exists, the unconsented
  primary account equals to the primary account. On desktop platforms (excl.
  ChromeOS), if no primary account exists and there exist any content-area
  accounts, the unconsented primary account equals to the first signed-in content-
  area account. In all other cases there is no unconsented primary account.
  NOTE: This name is still subject to finalization. The problem with
  "unconsented" in this context is that it means "did not consent"; really, this
  account is the "possibly unconsented, possibly primary, default account", which
  is a mouthful :).
- "OAuth2 tokens" are tokens related to the OAuth2 client-server authorization
  protocol. "OAuth2 refresh tokens" or just "refresh tokens" are long-lived
  tokens that the browser obtains via the user explicitly adding an account.
  Clients of IdentityManager do not explicitly see refresh tokens, but rather use
  IdentityManager to obtain "OAuth2 access tokens" or just "access tokens".
  Access tokens are short-lived tokens with given scopes that can be used to make
  authorized requests to Gaia endpoints.
- "The accounts with refresh tokens" refer to the accounts that are visible to
  IdentityManager with OAuth2 refresh tokens present (e.g., because the user has
    signed in to the browser and embedder-level code then added the account to
    IdentityManager, or because the user has added an account at the
    system level that embedder-level code then made visible to IdentityManager).
- PrimaryAccountTokenFetcher is the primary client-side interface for obtaining
  access tokens for the primary account. In particular, it can take care of
  waiting until the primary account is available.
- AccessTokenFetcher is the client-side interface for obtaining access tokens
  for arbitrary accounts; see access_token_fetcher.h for usage explanation and
  examples.
- IdentityTestEnvironment is the preferred test infrastructure for unittests
  of production code that interacts with IdentityManager. It is suitable for
  use in cases where neither the production code nor the unittest is interacting
  with Profile.
- identity_test_utils.h provides lower-level test facilities for interacting
  explicitly with IdentityManager. These facilities are the way to interact with
  IdentityManager in testing contexts where the production code and/or the
  unittest are interacting with Profile (in particular, where the
  IdentityManager instance with which the test is interacting must be
  IdentityManagerFactory::GetForProfile(profile)). Examples include integration
  tests and Profile-based unittests (in the latter case, consider migrating the
  test and production code away from using Profile directly and using
  IdentityTestEnvironment).
- Various mutators of account state are available through IdentityManager (e.g.,
  PrimaryAccountMutator). These should in general be used only as part of larger
  embedder-specific flows for mutating the user's account state in ways that are
  in line with product specifications.

Documentation on the mapping between usage of legacy signin
classes (notably PrimaryAccountManager and ProfileOAuth2TokenService) and usage
of IdentityManager is available here:

https://docs.google.com/document/d/14f3qqkDM9IE4Ff_l6wuXvCMeHfSC9TxKezXTCyeaPUY/edit#
