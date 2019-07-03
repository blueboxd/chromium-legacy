'use strict';

function verifySensorReading(pattern, values, timestamp, isNull) {
  function round(val) {
    return Number.parseFloat(val).toPrecision(6);
  }

  if (isNull) {
    return (values === null || values.every(r => r === null)) &&
           timestamp === null;
  }

  return values.every((r, i) => round(r) === round(pattern[i])) &&
         timestamp !== null;
}

function verifyXyzSensorReading(pattern, {x, y, z, timestamp}, isNull) {
  return verifySensorReading(pattern, [x, y, z], timestamp, isNull);
}

function verifyQuatSensorReading(pattern, {quaternion, timestamp}, isNull) {
  return verifySensorReading(pattern, quaternion, timestamp, isNull);
}

function verifyAlsSensorReading(pattern, {illuminance, timestamp}, isNull) {
  return verifySensorReading(pattern, [illuminance], timestamp, isNull);
}
