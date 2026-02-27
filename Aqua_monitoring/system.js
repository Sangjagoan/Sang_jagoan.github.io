
/* ========================================
 SYSTEM MONITORING
======================================== */
async function loadSystemStatus() {

    try {
        const res = await fetch("/system/ui");
        const data = await res.json();
        updateHealthUI(data);
        document.getElementById("uptimeValue").textContent = formatUptime(data.uptime);

        document.getElementById("heapValue").textContent = (data.heap / 1024).toFixed(1) + " KB";
        document.getElementById("minHeapValue").textContent = (data.minHeap / 1024).toFixed(1) + " KB";
        document.getElementById("maxBlockValue").textContent = (data.maxBlock / 1024).toFixed(1) + " KB";

        const percent = (data.heap / 320000) * 100;
        document.getElementById("heapBar").style.width = percent + "%";

        document.getElementById("plcStateValue").textContent = data.state;
        document.getElementById("sensorValue").textContent = data.sensor;
        document.getElementById("wdtValue").textContent = data.wdt;
        document.getElementById("ntpValue").textContent = data.ntp;

        document.getElementById("debugConsole").textContent =
            JSON.stringify(data, null, 2);

    } catch (e) {
        console.log("System fetch error");
    }
}

function updateHealthUI(data) {

    const el = document.getElementById("sysHealth");

    if (!el) return;

    el.innerText = data.health;

    el.classList.remove("green", "yellow", "red");

    if (data.health === "HEALTHY") el.classList.add("green");
    else if (data.health === "WARNING") el.classList.add("yellow");
    else el.classList.add("red");
}

function formatUptime(seconds) {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    return `${h}h ${m}m ${s}s`;
}

let socket;

function initWebSocket() {

    socket = new WebSocket(`ws://${location.hostname}:81/`);

    socket.onopen = () => {
        console.log("WebSocket connected");
    };

    socket.onmessage = (event) => {
        const data = JSON.parse(event.data);
        updateSystemUI(data);
    };

    socket.onclose = () => {
        console.log("WebSocket closed, reconnecting...");
        setTimeout(initWebSocket, 2000);
    };
}

function updateSystemUI(data) {

    document.getElementById("uptimeValue").textContent =
        formatUptime(data.uptime);


    document.getElementById("heapValue").textContent =
        (data.heap / 1024).toFixed(1) + " KB";

    document.getElementById("minHeapValue").textContent =
        (data.minHeap / 1024).toFixed(1) + " KB";

    document.getElementById("maxBlockValue").textContent =
        (data.maxBlock / 1024).toFixed(1) + " KB";

    updateHealthBadge(data.health);
}

document.addEventListener("DOMContentLoaded", () => {
    loadSystemStatus();   // sekali saja
    initWebSocket();      // realtime stream
});

