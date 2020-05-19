function areArrayBuffersEqual(buffer1, buffer2)
{
  if (buffer1.byteLength !== buffer2.byteLength) {
    return false;
  }
  let array1 = new Int8Array(buffer1);
  var array2 = new Int8Array(buffer2);
  for (let i = 0 ; i < buffer1.byteLength ; ++i) {
    if (array1[i] !== array2[i]) {
      return false;
    }
  }
  return true;
}

function areMetadataEqual(metadata1, metadata2) {
  return metadata1.synchronizationSource === metadata2.synchronizationSource;
}

function areFrameInfosEqual(frame1, frame2) {
  return frame1.timestamp === frame2.timestamp &&
         areMetadataEqual(frame1.getMetadata(), frame2.getMetadata()) &&
         areArrayBuffersEqual(frame1.data, frame2.data);
}

async function exchangeOfferAnswer(pc1, pc2) {
  const offer = await pc2.createOffer({offerToReceiveAudio: true, offerToReceiveVideo: true});
  // TODO(crbug.com/1066819): remove this hack when we do not receive duplicates from RTX
  //   anymore.
  // Munge the SDP to disable bandwidth probing via RTX.
  const sdp = offer.sdp.replace(new RegExp('rtx', 'g'), 'invalid');
  await pc2.setLocalDescription(offer);
  await pc1.setRemoteDescription({type: 'offer', sdp});

  const answer = await pc1.createAnswer();
  await pc1.setLocalDescription(answer);
  await pc2.setRemoteDescription(answer);
}

async function exchangeOfferAnswerReverse(pc1, pc2) {
  const offer = await pc2.createOffer({offerToReceiveAudio: true, offerToReceiveVideo: true});
  // Munge the SDP to disable bandwidth probing via RTX.
  const sdp = offer.sdp.replace(new RegExp('rtx', 'g'), 'invalid');
  await pc1.setRemoteDescription({type: 'offer', sdp});
  await pc2.setLocalDescription(offer);

  const answer = await pc1.createAnswer();
  await pc2.setRemoteDescription(answer);
  await pc1.setLocalDescription(answer);
}
