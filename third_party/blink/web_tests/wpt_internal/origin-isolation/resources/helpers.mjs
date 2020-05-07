export function insertIframe(hostname, headerOrder) {
  const url = new URL("send-headers.py", import.meta.url);
  url.hostname = hostname;
  url.searchParams.set("headerOrder", headerOrder);

  const iframe = document.createElement("iframe");
  iframe.src = url.href;

  return new Promise((resolve, reject) => {
    iframe.onload = () => resolve(iframe.contentWindow);
    iframe.onerror = () => reject(new Error(`Could not load ${iframe.src}`));
    document.body.append(iframe);
  });
}
