'use strict';

/**
 * Allows to override specific interface request handlers for
 * DocumentInterfaceBroker for testing purposes
 * @param {Object} overrides an object where the keys are names of
 *     DocumentInterfaceBroker's methods and the values are corresponding handler
 *     functions taking a request parameter and binding its handle to the relevant
 *     testing implementation.
 * Example:
 *   const testFooImpl = new FooInterfaceCallbackRouter;
 *   ... override FooInterface methods ...
 *   setDocumentInterfaceBrokerOverrides({getFooInterface: request => {
 *     testFooImpl.$.bindHandle(request.handle);
 *   }});
 */
function setDocumentInterfaceBrokerOverrides(overrides) {
  const {handle0, handle1} = Mojo.createMessagePipe();
  const realBrokerRemote = new blink.mojom.DocumentInterfaceBrokerRemote(
      Mojo.replaceDocumentInterfaceBrokerForTesting(handle0));

  for (const method of Object.keys(overrides)) {
    realBrokerRemote[method] = overrides[method];
  }

  // Use the real broker (with overrides) as the implementation of the JS-side broker
  const testBrokerReceiver =
    new blink.mojom.DocumentInterfaceBrokerReceiver(realBrokerRemote);
  testBrokerReceiver.$.bindHandle(handle1);
}
