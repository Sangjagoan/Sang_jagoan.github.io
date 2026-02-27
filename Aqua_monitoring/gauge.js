"use strict";

/* ========================================
   PRESSURE CANDLE ENGINE
======================================== */

const PSI_CANDLE = createCandle();
const KG_CANDLE = createCandle();

function createCandle() {
    return {
        currentHour: null,
        open: 0,
        high: 0,
        low: 0,
        close: 0,
        candles: []
    };
}

function pushPressureCandle(engine, value) {

    const now = getCurrentServerTime();
    const hourKey = now.getFullYear() + "-" +
        now.getMonth() + "-" +
        now.getDate() + "-" +
        now.getHours();

    if (engine.currentHour !== hourKey) {

        if (engine.currentHour !== null) {
            engine.candles.push({
                o: engine.open,
                h: engine.high,
                l: engine.low,
                c: engine.close
            });

            if (engine.candles.length > 24)
                engine.candles.shift();
        }

        engine.currentHour = hourKey;
        engine.open = value;
        engine.high = value;
        engine.low = value;
        engine.close = value;

    } else {

        engine.high = Math.max(engine.high, value);
        engine.low = Math.min(engine.low, value);
        engine.close = value;
    }
}

function drawPressureCandles(engine, svgId) {

    const svg = document.getElementById(svgId);
    if (!svg) return;

    svg.innerHTML = "";

    const candles = engine.candles;
    if (candles.length < 1) return;

    const width = 300;
    const height = 200;
    const candleWidth = 15;
    const gap = 10;

    const allValues = candles.flatMap(c => [c.o, c.h, c.l, c.c]);
    const min = Math.min(...allValues);
    const max = Math.max(...allValues);

    function scale(v) {
        if (max === min) return height / 2;
        return height - ((v - min) / (max - min)) * 150;
    }

    candles.forEach((c, i) => {

        const x = i * (candleWidth + gap) + 30;

        const yOpen = scale(c.o);
        const yClose = scale(c.c);
        const yHigh = scale(c.h);
        const yLow = scale(c.l);

        const color = c.c >= c.o ? "#6b8e6b" : "#c27878";

        const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
        line.setAttribute("x1", x + candleWidth / 2);
        line.setAttribute("x2", x + candleWidth / 2);
        line.setAttribute("y1", yHigh);
        line.setAttribute("y2", yLow);
        line.setAttribute("stroke", color);
        line.setAttribute("stroke-width", "2");
        svg.appendChild(line);

        const rect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
        rect.setAttribute("x", x);
        rect.setAttribute("y", Math.min(yOpen, yClose));
        rect.setAttribute("width", candleWidth);
        rect.setAttribute("height", Math.abs(yClose - yOpen) || 1);
        rect.setAttribute("fill", color);
        svg.appendChild(rect);
    });
}

function updatePressureGauges(psi, kg) {

    updateGauge("psiNeedle", "psiValue", psi, 0, 50);
    updateGauge("kgNeedle", "kgValue", kg, 0, 3);
}

function updateGauge(needleId, valueId, value, min, max) {

    const needle = document.getElementById(needleId);
    const valueText = document.getElementById(valueId);

    if (!needle) return;

    const percent = (value - min) / (max - min);
    const clamped = Math.max(0, Math.min(percent, 1));
    const angle = -90 + (clamped * 180);

    // Smooth animation
    needle.style.transition = "transform 0.3s ease-out";
    needle.setAttribute("transform", `rotate(${angle} 150 150)`);

    if (valueText) valueText.textContent = value.toFixed(1);

    // LED + Danger Zone Logic
    const led = document.getElementById("psiLED");
}

function rotateNeedle(id, value, min, max) {

    const needle = document.getElementById(id);
    if (!needle) return;

    const percent = (value - min) / (max - min);
    const clamped = Math.max(0, Math.min(percent, 1));

    // Sudut lebih lebar & natural
    const angle = -110 + (clamped * 220);

    needle.style.transition = "transform 0.4s cubic-bezier(.4,2,.6,1)";
    needle.setAttribute("transform", `rotate(${angle} 150 150)`);
}


function setPFStatus(id, value) {

    const val = document.getElementById(id);
    const icon = document.getElementById("icon-" + id);

    if (!val || !icon) return;

    val.classList.remove("status-good", "status-warn", "status-bad");
    icon.classList.remove("status-good", "status-warn", "status-bad");

    let cls;
    if (value >= 0.95) cls = "status-good";
    else if (value >= 0.75) cls = "status-warn";
    else cls = "status-bad";

    val.classList.add(cls);
    icon.classList.add(cls);
}

function updatePSI(value) {
    const max = 100;
    const percent = value / max;
    const dash = percent * 251;
    const angle = percent * 180;

    const psiArc = document.querySelector(".gauge path:nth-of-type(2)");
    const needle = document.querySelector(".gauge line");

    psiArc.setAttribute("stroke-dasharray", dash + " 251");
    needle.setAttribute("transform", "rotate(" + angle + " 100 100)");

    document.querySelector(".card:nth-child(1) .gauge-value")
        .textContent = value.toFixed(1) + " PSI";
}

function openAnalytics(dataArray, label, unit) {

    const panel = document.getElementById("analyticsPanel");
    if (!panel || !dataArray || dataArray.length === 0) return;

    const values = dataArray.map(x => x.v ?? x);

    const avg = values.reduce((a, b) => a + b, 0) / values.length;
    const max = Math.max(...values);
    const min = Math.min(...values);

    document.getElementById("analyticsAvg").textContent = avg.toFixed(2) + " " + unit;
    document.getElementById("analyticsMax").textContent = max.toFixed(2) + " " + unit;
    document.getElementById("analyticsMin").textContent = min.toFixed(2) + " " + unit;

    document.getElementById("analyticsStatus").textContent =
        avg > max * 0.8 ? "High Load" :
            avg < max * 0.3 ? "Low Activity" :
                "Normal";

    panel.classList.add("active");
}

function closeAnalytics() {
    const panel = document.getElementById("analyticsPanel");
    if (panel) panel.classList.remove("active");
}

function createTicks(groupId, min, max, stepMajor, stepMinor) {

    const group = document.getElementById(groupId);
    if (!group) return;

    const centerX = 150;
    const centerY = 150;
    const radius = 120;

    group.innerHTML = "";

    for (let v = min; v <= max; v += stepMinor) {

        const percent = (v - min) / (max - min);
        const angle = (-180 + percent * 180) * Math.PI / 180;

        const inner = radius - 15;
        const outer = radius;

        const x1 = centerX + inner * Math.cos(angle);
        const y1 = centerY + inner * Math.sin(angle);

        const x2 = centerX + outer * Math.cos(angle);
        const y2 = centerY + outer * Math.sin(angle);

        const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
        line.setAttribute("x1", x1);
        line.setAttribute("y1", y1);
        line.setAttribute("x2", x2);
        line.setAttribute("y2", y2);

        if (v % stepMajor === 0) {
            line.setAttribute("stroke", "#fff");
            line.setAttribute("stroke-width", "3");

            // NUMBER
            const textRadius = radius - 45;
            const tx = centerX + textRadius * Math.cos(angle);
            const ty = centerY + textRadius * Math.sin(angle);

            const text = document.createElementNS("http://www.w3.org/2000/svg", "text");
            text.setAttribute("x", tx);
            text.setAttribute("y", ty + 5);
            text.setAttribute("text-anchor", "middle");
            text.setAttribute("fill", "#fff");
            text.setAttribute("font-size", "14");
            text.textContent = v;

            group.appendChild(text);

        } else {
            line.setAttribute("stroke", "#aaa");
            line.setAttribute("stroke-width", "1.5");
        }

        group.appendChild(line);
    }
}

function createDangerArc(id, min, max, dangerStart, dangerEnd) {

    const arc = document.getElementById(id);
    if (!arc) return;

    const centerX = 150;
    const centerY = 150;
    const radius = 120;   // sama dengan arc utama

    function polarToCartesian(angleDeg) {
        const rad = angleDeg * Math.PI / 180;
        return {
            x: centerX + radius * Math.cos(rad),
            y: centerY + radius * Math.sin(rad)
        };
    }

    const startPercent = (dangerStart - min) / (max - min);
    const endPercent = (dangerEnd - min) / (max - min);

    // SISTEM SAMA DENGAN NEEDLE & TICKS
    const startAngle = -90 + (startPercent * 180);
    const endAngle = -90 + (endPercent * 180);

    const start = polarToCartesian(startAngle);
    const end = polarToCartesian(endAngle);

    const largeArcFlag = (endAngle - startAngle) <= 180 ? 0 : 1;

    const d = `
        M ${start.x} ${start.y}
        A ${radius} ${radius} 0 ${largeArcFlag} 1 ${end.x} ${end.y}
    `;

    arc.setAttribute("d", d);
}

window.addEventListener("load", () => {
    createTicks("psiTicks", 0, 50, 10, 5);
    createTicks("kgTicks", 0, 3, 1, 0.5);
    createDangerArc("psiDangerArc", 0, 50, 40, 50); // 0	nilai minimum gauge 50	nilai maksimum gauge 40	awal zona bahaya 50	akhir zona bahaya
});
