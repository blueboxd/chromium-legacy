(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command addCredential validates parameters");

  const credentialId = "cred-1";
  const credentialOptions = {
    authenticatorId: "non-existant authenticator",
    credential: {
      credentialId: btoa(credentialId),
      privateKey: btoa("invalid private key"),
      rpIdHash: btoa("invalid hash"),
      signCount: 0,
    }
  };

  // Try without enabling the WebAuthn environment.
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try for an authenticator that does not exist.
  await dp.WebAuthn.enable();
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try with an invalid RP ID hash.
  credentialOptions.authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));

  // Try with a private key that is not valid.
  credentialOptions.credential.rpIdHash =
      await session.evaluateAsync("generateRpIdHash()");
  testRunner.log(await dp.WebAuthn.addCredential(credentialOptions));
  testRunner.completeTest();
})
