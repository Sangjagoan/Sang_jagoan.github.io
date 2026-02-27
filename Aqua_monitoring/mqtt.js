"use strict";

const MQTT_CONFIG = {

    host: "wss://c2ba5bec181b4ffab0e46ac233be0a3b.s1.eu.hivemq.cloud:8884/mqtt",

    username: "espuser",
    password: "Esp32Cloud2026!",

    topic_data: "esp32/panel/data",
    topic_state: "esp32/panel/state",
    topic_heartbeat: "esp32/panel/heartbeat"
};

let mqttClient = null;

function mqttStart()
{
    console.log("Starting MQTT...");

    mqttClient = mqtt.connect(MQTT_CONFIG.host, {

        username: MQTT_CONFIG.username,
        password: MQTT_CONFIG.password,
        reconnectPeriod: 3000,
        connectTimeout: 10000,
        clean: true
    });

    mqttClient.on("connect", () =>
    {
        console.log("MQTT Connected ✅");

        mqttClient.subscribe(MQTT_CONFIG.topic_data);
        mqttClient.subscribe(MQTT_CONFIG.topic_state);
        mqttClient.subscribe(MQTT_CONFIG.topic_heartbeat);
    });

    mqttClient.on("message", mqttOnMessage);

    mqttClient.on("error", err =>
    {
        console.error("MQTT error ❌:", err);
    });
}

function mqttOnMessage(topic, payload)
{
    try
    {
        const data = JSON.parse(payload.toString());

        onMQTTData(topic, data);

    }
    catch(e)
    {
        console.error("JSON error", e);
    }
}