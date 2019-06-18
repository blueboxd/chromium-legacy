(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command addCredential works");

  // Create an authenticator.
  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  // Register a credential.
  const credentialId = "cred-1";
  const addCredentialResult = (await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId: btoa(credentialId),
      rpIdHash: await session.evaluateAsync("generateRpIdHash()"),
      privateKey: await session.evaluateAsync("generateBase64Key()"),
      signCount: 0,
    }
  }));
  testRunner.log(addCredentialResult);

  // Authenticate with the registered credential.
  testRunner.log(await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: new TextEncoder().encode("${credentialId}"),
    transports: ["usb", "ble", "nfc"],
  })`));

  testRunner.completeTest();
})
