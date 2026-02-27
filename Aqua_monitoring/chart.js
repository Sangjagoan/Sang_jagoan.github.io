"use strict";
let serverOffset = 0;
let lastServerTime = null;
/* ========================================
   Full Pages
======================================== */
function initCardFullscreen() {

    document.querySelectorAll(".card").forEach(card => {

        card.addEventListener("click", (e) => {

            if (e.target.closest("button, .chart-tab")) return;

            const active = document.querySelector(".card-fullscreen");

            if (active && active !== card) {
                active.classList.remove("card-fullscreen");
                document.querySelector(".card-overlay")?.remove();
            }

            if (card.classList.contains("card-fullscreen")) {
                exitFullscreen(card);
            } else {
                enterFullscreen(card);
            }
        });
    });

    document.addEventListener("keydown", (e) => {
        if (e.key === "Escape") {
            const active = document.querySelector(".card-fullscreen");
            if (active) exitFullscreen(active);
        }
    });

    function enterFullscreen(card) {
        const overlay = document.createElement("div");
        overlay.className = "card-overlay";
        overlay.onclick = () => exitFullscreen(card);
        document.body.appendChild(overlay);
        card.classList.add("card-fullscreen");
    }

    function exitFullscreen(card) {
        card.classList.remove("card-fullscreen");
        document.querySelector(".card-overlay")?.remove();
    }
}

/* ========================================
   VOLTAGE CHART ENGINE (2 PZEM)
======================================== */
function updateChartLabels(chart, containerId) {

    const container = document.getElementById(containerId);
    if (!container) return;

    container.innerHTML = "";

    const now = lastServerTime ? new Date(lastServerTime) : new Date();
    const labels = [];

    const formatHour = (d) =>
        String(d.getHours()).padStart(2, "0") + ":00";

    const formatMinute = (d) =>
        String(d.getHours()).padStart(2, "0") + ":" +
        String(d.getMinutes()).padStart(2, "0");

    if (chart.mode === "1D") {

        const base = getCurrentServerTime();
        base.setMilliseconds(0);

        for (let i = 5; i >= 0; i--) {
            const t = new Date(base - i * 10000);
            labels.push(
                String(t.getMinutes()).padStart(2, "0") + ":" +
                String(t.getSeconds()).padStart(2, "0")
            );
        }
    }

    if (chart.mode === "1M") {
        const base = new Date(now);
        base.setMinutes(0, 0, 0);

        for (let i = 5; i >= 0; i--) {
            const t = new Date(base - i * 3600000);
            labels.push(formatHour(t));
        }
    }

    if (chart.mode === "1W") {
        const base = new Date(now);
        base.setSeconds(0, 0);

        for (let i = 5; i >= 0; i--) {
            const t = new Date(base - i * 60000);
            labels.push(formatMinute(t));
        }
    }

    if (chart.mode === "1Y") {
        const base = new Date(now);
        base.setHours(0, 0, 0, 0);

        for (let i = 6; i >= 0; i--) {
            const t = new Date(base - i * 86400000);
            labels.push(
                t.getDate() + "/" + (t.getMonth() + 1)
            );
        }
    }

    labels.forEach(label => {
        const span = document.createElement("span");
        span.textContent = label;
        container.appendChild(span);
    });
}

function getCurrentServerTime() {
    return new Date(Date.now() + serverOffset);
}

function updateLiveDateTime() {

    const now = getCurrentServerTime();

    const days = [
        "Minggu", "Senin", "Selasa",
        "Rabu", "Kamis", "Jumat", "Sabtu"
    ];

    const dayName = days[now.getDay()];
    const date = now.getDate();
    const month = now.getMonth() + 1;
    const year = now.getFullYear();

    const time =
        String(now.getHours()).padStart(2, "0") + ":" +
        String(now.getMinutes()).padStart(2, "0") + ":" +
        String(now.getSeconds()).padStart(2, "0");

    const dateEl = document.getElementById("liveDate");
    const clockEl = document.getElementById("liveClock");

    if (dateEl)
        dateEl.textContent = `${dayName}, ${date}/${month}/${year}`;

    if (clockEl)
        clockEl.textContent = time;
}

function createChart() {
    return {
        mode: "1D",
        raw: [],
        min: [],
        hour: [],
        day: []
    };
}

let lastSave = 0;
const CHART1 = createChart();
const CHART2 = createChart();
const CHART3 = createChart();

function pushVoltage(chart, v) {

    chart.raw.push({
        t: getCurrentServerTime().getTime(),
        v
    });

    trim(chart.raw, 86400);

    if (!chart._m || Date.now() - chart._m > 60000) {
        chart._m = Date.now();
        chart.min.push(avg(chart.raw, 60));
        trim(chart.min, 10080);
    }

    if (!chart._h || Date.now() - chart._h > 3600000) {
        chart._h = Date.now();
        chart.hour.push(avg(chart.min, 60));
        trim(chart.hour, 720);
    }

    if (!chart._d || Date.now() - chart._d > 86400000) {
        chart._d = Date.now();
        chart.day.push(avg(chart.hour, 24));
        trim(chart.day, 365);
    }

}

function avg(arr, n) {
    const s = arr.slice(-n);
    return s.reduce((a, b) => a + (b.v ?? b), 0) / s.length || 0;
}

function trim(arr, max) {
    if (arr.length > max) arr.splice(0, arr.length - max);
}

function drawChart(chart, lineId, areaId, labelId, labelContainerId) {

    let data;

    if (chart.mode === "1D") data = chart.raw.map(x => x.v);
    if (chart.mode === "1W") data = chart.min;
    if (chart.mode === "1M") data = chart.hour;
    if (chart.mode === "1Y") data = chart.day;

    const maxPoints = 60;
    data = data.slice(-maxPoints);

    if (!data || data.length < 2) return;

    const svg = document.getElementById(lineId).ownerSVGElement;
    const width = svg.viewBox.baseVal.width;
    const height = svg.viewBox.baseVal.height;
    const minVolt = 150;
    const maxVolt = 260;

    const step = width / (maxPoints - 1);

    let line = "", area = "";

    data.forEach((v, i) => {
        let x = i * step;
        let y = height - ((v - minVolt) / (maxVolt - minVolt)) * height;

        line += `${x},${y} `;
        area += `${x},${y} `;
    });

    if (labelContainerId) {
        updateChartLabels(chart, labelContainerId);
    }

    area += `${(data.length - 1) * step},200 0,200`;

    const lineEl = document.getElementById(lineId);
    const areaEl = document.getElementById(areaId);
    const labelGroup = document.getElementById(labelId);

    if (!lineEl || !areaEl) return;

    lineEl.setAttribute("points", line);
    areaEl.setAttribute("points", area);

    if (!labelGroup) return;

    const fragment = document.createDocumentFragment();

    for (let i = 0; i < data.length; i += 8) {

        const v = data[i];

        const text = document.createElementNS("http://www.w3.org/2000/svg", "text");

        const x = i * step;
        const y = height - ((v - minVolt) / (maxVolt - minVolt)) * 150;

        text.setAttribute("x", x);
        text.setAttribute("y", y - 8);
        text.setAttribute("text-anchor", "middle");
        text.setAttribute("font-size", "11");
        text.setAttribute("fill", "#ffffff");
        text.setAttribute("opacity", "0.8");

        text.textContent = v.toFixed(1);

        fragment.appendChild(text);
    }

    labelGroup.innerHTML = "";
    labelGroup.appendChild(fragment);
}

document.querySelectorAll(".chart-tab").forEach(btn => {
    btn.onclick = () => {

        const chartId = btn.dataset.chart;
        const mode = btn.dataset.mode;

        const container = btn.parentElement;
        container.querySelectorAll(".chart-tab")
            .forEach(x => x.classList.remove("active"));

        btn.classList.add("active");

        if (chartId === "1") {
            CHART1.mode = mode;
            drawChart(CHART1, "voltLine1", "voltArea1", "voltLabels1", "chartLabels1");
        }

        if (chartId === "2") {
            CHART2.mode = mode;
            drawChart(CHART2, "voltLine2", "voltArea2", "voltLabels2", "chartLabels2");
        }
        if (chartId === "3") {
            CHART3.mode = mode;
            drawChart(CHART3, "cmLine", "cmArea", "cmLabels", "chartLabels3");
        }
    }
});

function saveChartHistory() {

    localStorage.setItem("c1", JSON.stringify(CHART1));
    localStorage.setItem("c2", JSON.stringify(CHART2));
    localStorage.setItem("c3", JSON.stringify(CHART3));
}

function loadChartHistory() {

    const c1 = JSON.parse(localStorage.getItem("c1") || "null");
    const c2 = JSON.parse(localStorage.getItem("c2") || "null");
    const c3 = JSON.parse(localStorage.getItem("c3") || "null");

    if (c1) {
        CHART1.raw = c1.raw || [];
        CHART1.min = c1.min || [];
        CHART1.hour = c1.hour || [];
        CHART1.day = c1.day || [];
    }

    if (c2) {
        CHART2.raw = c2.raw || [];
        CHART2.min = c2.min || [];
        CHART2.hour = c2.hour || [];
        CHART2.day = c2.day || [];
    }

    if (c3) {
        CHART3.raw = c3.raw || [];
        CHART3.min = c3.min || [];
        CHART3.hour = c3.hour || [];
        CHART3.day = c3.day || [];
    }
}

function chartRenderLoop() {

    drawChart(CHART1, "voltLine1", "voltArea1", "voltLabels1", "chartLabels1");
    drawChart(CHART2, "voltLine2", "voltArea2", "voltLabels2", "chartLabels2");
    drawChart(CHART3, "cmLine", "cmArea", "cmLabels", "chartLabels3");

}

/* ========================================
           CHART AQUA MONITOR
======================================== */
function updateWaterDrop(id, value, max) {

    const percent = Math.min(value / max, 1);

    const dropHeight = 130;   // tinggi isi drop
    const startY = 150;       // posisi bawah drop

    const newHeight = dropHeight * percent;
    const newY = startY - newHeight;

    const fill = document.getElementById(id);

    fill.setAttribute("y", newY);
    fill.setAttribute("height", newHeight);
}

let currentLevel = 0;
let currentTinggi = 0;
let currentJarak = 0;

function updateAquaBars(L, T, J) {
    currentLevel = L;
    currentTinggi = T;
    currentJarak = J;

    drawWave("waveLevel", L, 100, 100);
    drawWave("waveTinggi", T, 239, 350);
    drawWave("waveJarak", J, 239, 600);

    const valueLevel = document.getElementById("valueLevel");
    const valueTinggi = document.getElementById("valueTinggi");
    const valueJarak = document.getElementById("valueJarak");

    if (valueLevel) valueLevel.textContent = L.toFixed(0) + "%";
    if (valueTinggi) valueTinggi.textContent = T.toFixed(0) + " cm";
    if (valueJarak) valueJarak.textContent = J.toFixed(1) + " cm";
}

let waveOffset = 0;

function drawWave(id, value, max, centerX) {

    const percent = Math.min(value / max, 1);

    const dropBottom = 210;
    const dropHeight = 150;

    const waterLevel = dropBottom - (dropHeight * percent);

    const amplitude = 5;      // tinggi gelombang
    const wavelength = 50;    // lebar gelombang

    let path = `M ${centerX - 60} ${dropBottom} `;

    for (let x = -60; x <= 60; x++) {
        const y = waterLevel +
            Math.sin((x + waveOffset) / wavelength * 2 * Math.PI) * amplitude;
        path += `L ${centerX + x} ${y} `;
    }

    path += `L ${centerX + 60} ${dropBottom} Z`;

    document.getElementById(id).setAttribute("d", path);
}

let lastFrame = 0;

function animateWave(timestamp) {

    if (timestamp - lastFrame < 33) {
        requestAnimationFrame(animateWave);
        return;
    }

    lastFrame = timestamp;

    if (!document.hidden) {
        waveOffset += 0.8;
        drawWave("waveLevel", currentLevel, 100, 100);
        drawWave("waveTinggi", currentTinggi, 239, 350);
        drawWave("waveJarak", currentTinggi, 239, 600);
    }

    requestAnimationFrame(animateWave);
}
// ================ Chart AquaMonitor End========================//

window.addEventListener("load", () => {
    requestAnimationFrame(animateWave);
    setInterval(updateLiveDateTime, 1000);
    initCardFullscreen();
    loadChartHistory();
    setInterval(saveChartHistory, 5000);
    setInterval(chartRenderLoop, 2000); // redraw tiap 1 detik

});

