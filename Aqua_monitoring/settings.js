/* ========================================
   Ubdate Firmware Manual
======================================== */
function formatBytes(bytes) {
    if (bytes === 0) return "0 Bytes";
    const k = 1024;
    const sizes = ["Bytes", "KB", "MB"];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return (bytes / Math.pow(k, i)).toFixed(2) + " " + sizes[i];
}

function uploadFile(fileInputId, progressId, statusId, infoId, endpoint) {

    const fileInput = document.getElementById(fileInputId);
    const file = fileInput.files[0];
    if (!file) return;

    document.getElementById(infoId).innerHTML =
        `File: ${file.name} (${formatBytes(file.size)})`;

    const xhr = new XMLHttpRequest();
    xhr.open("POST", endpoint, true);

    xhr.upload.onprogress = function (e) {
        if (e.lengthComputable) {
            let percent = (e.loaded / e.total) * 100;
            document.getElementById(progressId).style.width = percent + "%";
            document.getElementById(statusId).innerHTML = "Upload";
        }
    };

    xhr.onload = function () {
        if (xhr.status === 200) {
            document.getElementById(statusId).innerHTML = "✔ Update Successful";
            document.getElementById(statusId).className = "status success";
        } else {
            document.getElementById(statusId).innerHTML = "✖ Update Failed";
            document.getElementById(statusId).className = "status error";
        }
    };

    xhr.onerror = function () {
        document.getElementById(statusId).innerHTML = "✖ Upload Error";
        document.getElementById(statusId).className = "status error";
    };

    const formData = new FormData();
    formData.append("file", file);

    xhr.send(formData);
}

document.addEventListener("DOMContentLoaded", function () {

    // Firmware trigger
    const fwInput = document.getElementById("firmwareFile");
    const webInput = document.getElementById("webFile");

    if (fwInput) {
        fwInput.addEventListener("change", function () {
            uploadFile("firmwareFile", "fwProgress", "fwStatus", "fwFileInfo", "/update/firmware");
        });
    }

    // Web UI trigger
    if (webInput) {
        webInput.addEventListener("change", function () {
            uploadFile("webFile", "webProgress", "webStatus", "webFileInfo", "/upload/spiffs");
        });
    }

});

/* ========================================
   FIRMWARE OTA GITHUB
======================================== */
function startOnlineOTA() {

    const btn = document.getElementById("otaButton");
    btn.disabled = true;

    fetch("/ota")
        .then(r => r.text())
        .then(res => {

            if (res === "UP_TO_DATE") {
                setOTAStatus("Device already up to date", "success");
                return;
            }

            if (res === "START_OTA") {
                setOTAStatus("Starting OTA...", "info");
                monitorOTA();
            }
        })
        .catch(() => {
            setOTAStatus("Failed to start OTA", "error");
        });
}

function monitorOTA(btn) {

    const interval = setInterval(() => {

        fetch("/ota/status")
            .then(r => r.json())
            .then(data => {

                document.getElementById("otaProgressBar").style.width =
                    data.progress + "%";

                setOTAStatus(data.status, "info");

                if (data.status === "REBOOT" ||
                    data.status === "ERROR") {

                    clearInterval(interval);
                    btn.disabled = false;   // 🔥 enable lagi
                }
            });

    }, 1000);
}

function setOTAStatus(text, type) {
    const el = document.getElementById("otaStatusText");
    el.textContent = text;
    el.className = "status " + type;
}

function loadFirmwareVersionSafe() {
    const el = document.getElementById("fwVersion");
    if (!el) return;

    fetch("/version")
        .then(r => r.json())
        .then(data => {
            el.textContent = "v" + data.version;
        })
        .catch(console.error);
}


/* ========================================
   RESET PZEM
======================================== */
function initResetPzem() {

    const btn = document.getElementById("resetPzemBtn");
    const statusEl = document.getElementById("maintenanceStatus");

    if (!btn) return; // kalau bukan di halaman settings, stop

    btn.addEventListener("click", async function () {

        if (!confirm("Are you sure you want to reset PZEM energy data?")) return;

        btn.disabled = true;

        if (statusEl) {
            statusEl.textContent = "Sending reset command...";
            statusEl.className = "status";
        }

        try {
            const response = await fetch("/reset/pzem", { method: "POST" });
            const text = await response.text();

            if (text === "OK") {
                if (statusEl) {
                    statusEl.textContent = "PZEM reset successful";
                    statusEl.className = "status success";
                }
            } else {
                if (statusEl) {
                    statusEl.textContent = "Reset failed";
                    statusEl.className = "status error";
                }
            }

        } catch (err) {

            if (statusEl) {
                statusEl.textContent = "Connection error";
                statusEl.className = "status error";
            }

        }

        btn.disabled = false;
    });
}

async function save() {
    const ssid = document.getElementById("wifiSSID").value;
    const pass = document.getElementById("wifiPASS").value;

    if (!ssid) {
        alert("SSID tidak boleh kosong!");
        return;
    }

    // Tampilkan loader
    document.getElementById("wifiConnectingBox").style.display = "block";
    document.getElementById("wifiConnectingText").innerText =
        "Mencoba menghubungkan ke WiFi: " + ssid + " ...";

    const resultBox = document.getElementById("wifiResult");
    resultBox.innerText = "";
    resultBox.className = "";

    fetch('/wifi/save', {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
            ssid: ssid,
            password: pass
        })
    })
        .then(r => r.text())
        .then(msg => {

            // Sembunyikan loader
            document.getElementById("wifiConnectingBox").style.display = "none";

            if (msg === "OK_CONNECTED") {
                resultBox.innerText = "✅ WiFi berhasil terhubung! ESP32 akan restart...";
                resultBox.className = "success";
            }
            else if (msg === "FAIL_CONNECT") {   // 🔹 DIBETULKAN
                resultBox.innerText = "❌ Gagal konek! Periksa SSID / Password.";
                resultBox.className = "fail";
            }
            else {
                resultBox.innerText = "⚠️ Respons tidak dikenal: " + msg;
                resultBox.className = "fail";
            }
        })
        .catch(err => {
            document.getElementById("wifiConnectingBox").style.display = "none";
            resultBox.innerText =
                "❌ Tidak dapat mengirim data ke ESP32! Pastikan Anda terhubung ke AP ESP32.";
            resultBox.className = "fail";
        });
}

async function scanWiFi() {
    const btn = document.querySelector('[onclick="scanWiFi()"]');
    btn.disabled = true;

    const ssidSelect = document.getElementById("wifiSSID");

    // Tampilkan loading sementara
    ssidSelect.innerHTML = "<option> Scanning...</option>";

    try {
        const res = await fetch("/wifi/scan");
        const networks = await res.json();

        ssidSelect.innerHTML = "<option value=''></option>";

        networks.forEach(net => {
            const opt = document.createElement("option");
            opt.value = net.ssid;
            opt.textContent = `${net.ssid} (${net.rssi} dBm)`;
            ssidSelect.appendChild(opt);
        });

    } catch (err) {
        ssidSelect.innerHTML = "<option>Scan Failed</option>";
        console.error("WiFi scan error:", err);
    }

    btn.disabled = false;
}

function updateWifiIcon(rssi, connected) {

    const arc2 = document.getElementById("arc2");
    const arc3 = document.getElementById("arc3");
    const arc4 = document.getElementById("arc4");
    const dot = document.getElementById("dot");

    if (!arc2 || !arc3 || !arc4 || !dot) return;

    // reset warna
    [arc2, arc3, arc4].forEach(a => a.setAttribute("stroke", "var(--text-muted)"));
    dot.setAttribute("fill", "var(--text-muted");

    if (!connected) return;

    let level = 0;

    if (rssi > -50) level = 4;
    else if (rssi > -60) level = 3;
    else if (rssi > -70) level = 2;
    else if (rssi > -80) level = 1;

    let color = "var(--text-primary)";
    if (rssi <= -70) color = "var(--los";
    if (rssi <= -80) color = "var(--wifi-diconnected)";

    // 🔥 DOT SELALU NYALA JIKA CONNECTED
    dot.setAttribute("fill", color);

    if (level >= 1) arc2.setAttribute("stroke", color);
    if (level >= 2) arc3.setAttribute("stroke", color);
    if (level >= 3) arc4.setAttribute("stroke", color);

}

async function updateTopWifiStatus() {

    try {
        const res = await fetch("/wifi/status");
        const data = await res.json();

        const text = document.getElementById("topWifiText");

        if (data.connected) {
            text.textContent = "Connected";
            text.style.color = "var(--text-primary)";
        } else {
            text.textContent = "Offline";
            text.style.color = "var( --wifi-diconnected)";
        }

        document.getElementById("SSID").textContent = data.ssid || "-";
        document.getElementById("wifiIP").textContent = data.ip || "-";
        document.getElementById("wifiRSSI").textContent = data.rssi + " dBm";
        // 🔥 INI YANG WAJIB ADA
        updateWifiIcon(data.rssi, data.connected);

    } catch (e) {
        console.log("WiFi top status error");
    }
}

// 🌙 Day / Night Mode Control
document.querySelectorAll('input[name="mode"]').forEach(radio => {
    radio.addEventListener("change", () => {

        const disabled = radio.value !== "AUTO" && radio.checked;

        document.getElementById("nightStart").disabled = disabled;
        document.getElementById("nightEnd").disabled = disabled;
    });
});

async function saveDayNightConfig() {

  const selected = document.querySelector('input[name="mode"]:checked').value;

  let mode = 0;
  if (selected === "DAY") mode = 1;
  if (selected === "NIGHT") mode = 2;

  const body = {
    mode: mode,
    nightStart: parseInt(document.getElementById("nightStart").value),
    nightEnd: parseInt(document.getElementById("nightEnd").value)
  };

  await fetch("/api/daynight", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  });

  loadDayNightConfig();
}

async function loadDayNightConfig() {

  const res = await fetch("/api/daynight");
  const data = await res.json();

  if (data.mode === 0)
    document.querySelector('input[value="AUTO"]').checked = true;

  if (data.mode === 1)
    document.querySelector('input[value="DAY"]').checked = true;

  if (data.mode === 2)
    document.querySelector('input[value="NIGHT"]').checked = true;

  document.getElementById("nightStart").value = data.nightStart;
  document.getElementById("nightEnd").value = data.nightEnd;

  updateStateIndicator(data.currentState);
}


window.addEventListener("load", () => {
    updateTopWifiStatus();
    loadDayNightConfig();
    loadFirmwareVersionSafe()
});
