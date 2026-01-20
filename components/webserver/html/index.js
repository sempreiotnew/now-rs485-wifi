async function scan() {
  const res = await fetch("/api/scan");
  const networks = await res.json();

  const ul = document.getElementById("list");
  ul.innerHTML = "";

  networks.forEach((n) => {
    const li = document.createElement("li");
    li.textContent = `${n.ssid} (${n.rssi} dBm)`;
    ul.appendChild(li);
  });
}

async function scan_now() {
  const res = await fetch("/api/nearby");
  const networks = await res.json();

  const ul = document.getElementById("listNow");
  ul.innerHTML = "";

  networks.forEach((n) => {
    const li = document.createElement("li");
    li.textContent = `${n.mac} (RSSI: ${n.rssi}) - last: ${n.last_msg}`;
    ul.appendChild(li);
  });
}
