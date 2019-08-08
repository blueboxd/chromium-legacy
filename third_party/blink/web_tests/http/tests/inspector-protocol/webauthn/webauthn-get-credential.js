(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command getCredential works");

  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: true,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  // TODO(nsatragno): content_shell does not support registering resident
  // credentials through navigator.credentials.create(). Update this test to use
  // registerCredential() once that feature is supported.
  const userHandle = btoa("nina");
  const credentialId = btoa("cred-1");
  await dp.WebAuthn.addCredential({
    authenticatorId,
    credential: {
      credentialId: credentialId,
      rpId: "devtools.test",
      privateKey: await session.evaluateAsync("generateBase64Key()"),
      signCount: 1,
      isResidentCredential: true,
      userHandle: userHandle,
    }
  });

  // Get the registered credential.
  let credential =
      (await dp.WebAuthn.getCredential({authenticatorId, credentialId})).result.credential;

  testRunner.log("credentialId: " + atob(credential.credentialId));
  testRunner.log("isResidentCredential: " + credential.isResidentCredential);
  testRunner.log("rpId: " + credential.rpId);
  testRunner.log("signCount: " + credential.signCount);
  testRunner.log("userHandle: " + atob(credential.userHandle));

  // We should be able to parse the private key.
  let keyData =
      Uint8Array.from(atob(credential.privateKey), c => c.charCodeAt(0)).buffer;
  let key = await window.crypto.subtle.importKey(
      "pkcs8", keyData, { name: "ECDSA", namedCurve: "P-256" },
      true /* extractable */, ["sign"]);

  testRunner.log(key);

  testRunner.completeTest();
})
