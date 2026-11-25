/**********************************************************************************
 * PROJET : AUTOMOTIVE SMART DASHBOARD & SECURE OTA (ISO 21434 / UN R156)
 * MODULE : PASSERELLE WI-FI / CAN / UART OTA
 * CARTE  : ESP32 DEVKIT V1
 *
 * CAN :
 *   GPIO 4 : CAN RX -> R du SN65HVD230
 *   GPIO 5 : CAN TX -> D du SN65HVD230
 *   Débit  : 500 kbps
 *
 * UART :
 *   GPIO 17 : TX2 -> D2 (PA10) NUCLEO-F446RE (115200 bauds)
 *   GPIO 16 : RX2
 **********************************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include "driver/twai.h"

// 1. CONFIGURATION WIFI
const char* ssid     = "louey";
const char* password = "123456789";

WebServer server(80);
bool wifiConnected = false;

// 2. CONFIGURATION CAN (TWAI) & UART
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

#define CAN_ID_OTA_NOTIFY 0x180
#define CAN_ID_OTA_CMD    0x181
#define CAN_ID_OTA_DATA   0x182
#define CAN_ID_OTA_CRC    0x183

#define UART_TX_PIN 17
#define UART_RX_PIN 16

bool canReady = false;
bool ota_in_progress = false;
bool uploadError = false;

uint32_t fileCRC = 0xFFFFFFFF;
uint32_t fileSize = 0;

// Émission CAN avec auto-récupération Bus-Off
bool sendCanMessage(uint32_t id, const uint8_t* data, uint8_t len) {
    if (!canReady || len > 8) return false;

    twai_status_info_t status_info;
    if (twai_get_status_info(&status_info) == ESP_OK) {
        if (status_info.state == TWAI_STATE_BUS_OFF) {
            twai_initiate_recovery();
            delay(10);
            twai_start();
        }
    }

    twai_message_t message = {};
    message.identifier = id;
    message.extd = 0;
    message.rtr = 0;
    message.data_length_code = len;
    for (uint8_t i = 0; i < len; i++) message.data[i] = data[i];

    return (twai_transmit(&message, pdMS_TO_TICKS(50)) == ESP_OK);
}

// Calcul CRC32
uint32_t updateCRC32(uint32_t crc, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return crc;
}

// 3. INTERFACE WEB SUR LE TELEPHONE (DESIGN SIMPLE ET EFFICACE)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Louey Krayem - OTA</title>
<style>
  body {
    font-family: Arial, sans-serif;
    background-color: #f0f2f5;
    margin: 0;
    padding: 20px;
    text-align: center;
  }
  .box {
    background: #ffffff;
    max-width: 400px;
    margin: 20px auto;
    padding: 25px 20px;
    border-radius: 10px;
    box-shadow: 0 4px 10px rgba(0,0,0,0.1);
  }
  h1 {
    color: #2e7d32;
    margin: 0 0 5px 0;
    font-size: 24px;
    font-weight: bold;
  }
  h2 {
    color: #555555;
    margin: 0 0 20px 0;
    font-size: 15px;
    font-weight: normal;
  }
  .file-input {
    margin: 15px 0;
    width: 100%;
    padding: 8px;
    border: 1px solid #ccc;
    border-radius: 5px;
    box-sizing: border-box;
  }
  .btn-upload {
    background-color: #1976d2;
    color: white;
    border: none;
    padding: 12px;
    width: 100%;
    border-radius: 5px;
    font-size: 15px;
    font-weight: bold;
    cursor: pointer;
    margin-top: 5px;
  }
  .btn-upload:active { background-color: #1565c0; }
  .btn-alert {
    background-color: #d32f2f;
    color: white;
    border: none;
    padding: 10px;
    width: 100%;
    border-radius: 5px;
    font-size: 14px;
    font-weight: bold;
    cursor: pointer;
    margin-top: 15px;
  }
  .btn-alert:active { background-color: #b71c1c; }
  .btn-reset {
    background-color: #757575;
    color: white;
    border: none;
    padding: 9px;
    width: 100%;
    border-radius: 5px;
    font-size: 13px;
    cursor: pointer;
    margin-top: 10px;
  }
  .btn-reset:active { background-color: #616161; }
  .progress {
    width: 100%;
    background-color: #e0e0e0;
    border-radius: 5px;
    margin-top: 15px;
    display: none;
    height: 12px;
    overflow: hidden;
  }
  .bar {
    height: 100%;
    width: 0%;
    background-color: #2e7d32;
    transition: width 0.2s;
  }
  .msg {
    margin-top: 15px;
    padding: 10px;
    background: #e8f5e9;
    border: 1px solid #c8e6c9;
    color: #2e7d32;
    border-radius: 5px;
    font-size: 13px;
  }
</style>
</head>
<body>

<div class="box">
  <h1>Louey Krayem</h1>
  <h2>Automotive Secure OTA</h2>

  <form id="form">
    <input type="file" id="binFile" name="update" accept=".bin" class="file-input">
    <button type="button" class="btn-upload" onclick="sendBinary()">Envoyer Mise a Jour (.bin)</button>
    <div id="prog" class="progress"><div id="bar" class="bar"></div></div>
  </form>

  <div id="msg" class="msg">Pret. Choisissez update.bin ou normal.bin</div>
</div>

<script>
function sendBinary() {
  var fileInput = document.getElementById('binFile');
  if (!fileInput.files || !fileInput.files[0]) {
    alert("Veuillez choisir un fichier .bin");
    return;
  }
  var file = fileInput.files[0];
  var data = new FormData();
  data.append("update", file);

  var prog = document.getElementById('prog');
  var bar = document.getElementById('bar');
  var msg = document.getElementById('msg');

  prog.style.display = 'block';
  bar.style.width = '0%';
  msg.innerText = "Envoi de " + file.name + "...";

  var xhr = new XMLHttpRequest();
  xhr.open("POST", "/update", true);

  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      var p = Math.round((e.loaded / e.total) * 100);
      bar.style.width = p + "%";
      msg.innerText = "Envoi : " + p + "%...";
    }
  };

  xhr.onload = function() {
    if (xhr.status == 200) {
      bar.style.width = '100%';
      msg.innerText = "Fichier envoye avec succes ! Regardez le STM32H7.";
    } else {
      msg.innerText = "Erreur : " + xhr.responseText;
    }
  };

  xhr.onerror = function() {
    msg.innerText = "Erreur de connexion.";
  };

  xhr.send(data);
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", index_html);
}

// 4. GESTION DE LA NOTIFICATION OTA (AVEC VERSION SÉLECTIONNÉE)
void handleNotify() {
    String ver = "v2.0.0";
    if (server.hasArg("ver")) {
        ver = server.arg("ver");
    }

    Serial.print("[OTA] Version sélectionnée par l'utilisateur : ");
    Serial.println(ver);

    // 1. Envoi sur la liaison série UART vers la NUCLEO (Broche GPIO 17 -> D2 Nucleo)
    Serial2.print("$CMD,OTA_ALERT,");
    Serial2.print(ver);
    Serial2.println();
    Serial.println("[UART] Commande transmise a la Nucleo sur GPIO 17");

    // 2. Envoi sur le bus CAN direct (TWAI GPIO 4 & 5)
    uint8_t payload[8] = { 0x02, 0x00, 0x00, 0xAA, 0x55, 0x12, 0x34, 0x56 };
    if (ver == "v1.0.0") {
        payload[0] = 0x01; // Version 1.0.0
    } else if (ver == "v1.5.0") {
        payload[0] = 0x01;
        payload[1] = 0x05;
    } else {
        payload[0] = 0x02; // Version 2.0.0 Sport
    }

    for (int k = 0; k < 5; k++) {
        sendCanMessage(CAN_ID_OTA_NOTIFY, payload, 8);
        delay(5);
    }
    Serial.println("[CAN] Trames ID 0x180 envoyees sur le bus CAN");

    String response = "Alerte OTA (" + ver + ") envoyée ! Toucher la notification en haut de l'écran H7 pour Accepter.";
    server.send(200, "text/plain", response);
}

// 5. GESTION DU TRANSFERT DE FICHIER BINAIRE
void handleFileUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.println();
        Serial.println("===== DEBUT OTA FICHIER BINAIRE (UN R156) =====");
        ota_in_progress = true;
        uploadError = false;
        fileCRC = 0xFFFFFFFF;
        fileSize = 0;

        String fname = upload.filename;
        fname.toLowerCase();
        bool isNormal = (fname.indexOf("normal") >= 0);

        if (isNormal) {
            Serial.println("[OTA] Detection de normal.bin -> Restauration du Firmware v1.0.0 Standard Eco !");
            uint8_t notifPayload[8] = { 0x01, 0x00, 0x00, 0xAA, 0x55, 0x12, 0x34, 0x56 };
            for (int k = 0; k < 15; k++) {
                sendCanMessage(0x190, notifPayload, 8); // ID 0x190 : Restauration et persistance V1
                delay(5);
            }
            Serial2.println("$RESET"); // Ordre à la Nucleo d'émettre les trames CAN 0x190
        } else {
            Serial.println("[OTA] Detection de update.bin -> Mise a jour vers le Firmware v2.0.0 Sport !");
            uint8_t notifPayload[8] = { 0x02, 0x00, 0x00, 0xAA, 0x55, 0x12, 0x34, 0x56 };
            for (int k = 0; k < 15; k++) {
                sendCanMessage(CAN_ID_OTA_NOTIFY, notifPayload, 8); // ID 0x180 : Notification MAJ V2
                delay(5);
            }
            Serial2.println("$OTA"); // Ordre à la Nucleo d'émettre les trames CAN 0x180
        }

        // Ordre CAN debut de transfert binaire
        uint8_t startPayload[8] = { 0x01, 0x01, 0, 0, 0, 0, 0, 0 };
        sendCanMessage(CAN_ID_OTA_CMD, startPayload, 8);
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        fileCRC = updateCRC32(fileCRC, upload.buf, upload.currentSize);
        fileSize += upload.currentSize;
    }
    else if (upload.status == UPLOAD_FILE_END) {
        ota_in_progress = false;
        uint32_t finalCRC = fileCRC ^ 0xFFFFFFFF;

        uint8_t crcPayload[8];
        crcPayload[0] = (finalCRC >> 24) & 0xFF;
        crcPayload[1] = (finalCRC >> 16) & 0xFF;
        crcPayload[2] = (finalCRC >> 8) & 0xFF;
        crcPayload[3] = finalCRC & 0xFF;
        crcPayload[4] = (fileSize >> 24) & 0xFF;
        crcPayload[5] = (fileSize >> 16) & 0xFF;
        crcPayload[6] = (fileSize >> 8) & 0xFF;
        crcPayload[7] = fileSize & 0xFF;

        sendCanMessage(CAN_ID_OTA_CRC, crcPayload, 8);
        Serial.println("===== FIN OTA FICHIER BINAIRE =====");
    }
    else if (upload.status == UPLOAD_FILE_ABORTED) {
        ota_in_progress = false;
    }
}

void handleUpdate() {
    server.send(200, "text/plain", "Fichier envoye avec succes ! Regardez le STM32H7.");
}

// 6. INITIALISATION

// Handler pour alerte directe (SANS MISE A JOUR)
void handleDirectAlert() {
    Serial.println("[ALERTE] Emission Alerte Vehicule Directe (ID 0x170)...");
    uint8_t alertPayload[8] = { 0xAA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    for (int k = 0; k < 15; k++) {
        sendCanMessage(0x170, alertPayload, 8);
        delay(5);
    }
    Serial2.println("$ALERT"); // Ordre à la Nucleo d'émettre les trames CAN 0x170 (Alerte rouge seule)
    server.send(200, "text/plain", "⚠️ Alerte Véhicule affichée sur le tableau de bord (Sans mise à jour).");
}

// Handler pour réinitialiser la démo vers v1.0.0
void handleResetDemo() {
    Serial.println("[RESET] Reinitialisation vers v1.0.0 (ID 0x190)...");
    uint8_t resetPayload[8] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    for (int k = 0; k < 15; k++) {
        sendCanMessage(0x190, resetPayload, 8);
        delay(5);
    }
    Serial2.println("$RESET"); // Ordre à la Nucleo d'émettre les trames CAN 0x190 (Reset V1 vert)
    server.send(200, "text/plain", "↺ Démo réinitialisée en mode V1 Standard.");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // Initialisation UART2 vers Nucleo (Broche GPIO 17 = TX)
    Serial2.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    // Initialisation CAN TWAI
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    g_config.tx_queue_len = 20;
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
        canReady = true;
        Serial.println("CAN TWAI 500 kbps : DEMARRE");
    }

    // Scan des reseaux Wi-Fi visibles pour forcer la detection du telephone
    Serial.println("\nScan des reseaux Wi-Fi...");
    int n = WiFi.scanNetworks();
    bool found = false;
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) == ssid) {
            found = true;
            Serial.printf("Point d'acces '%s' trouve ! (Canal: %d, Signal: %d dBm)\n", ssid, WiFi.channel(i), WiFi.RSSI(i));
            break;
        }
    }
    if (!found) {
        Serial.printf("ATTENTION : Le point d'acces '%s' n'est PAS visible par l'ESP32 !\n", ssid);
        Serial.println("-> Verifiez que le partage est bien en 2.4 GHz et que 'Maximiser la compatibilite' est active sur le telephone.");
    }

    // Connexion Wi-Fi au téléphone
    WiFi.disconnect(true);
    delay(200);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\nWIFI CONNECTE !");
        Serial.print("Adresse IP : http://");
        Serial.println(WiFi.localIP());
    } else {
        // Mode Point d'Accès de secours si partage téléphone éteint
        WiFi.mode(WIFI_AP);
        WiFi.softAP("louey_ota", "123456789");
        Serial.println("\nPoint d'acces actif : louey_ota (192.168.4.1)");
        wifiConnected = true;
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/notify", HTTP_GET, handleNotify);
    server.on("/update", HTTP_POST, handleUpdate, handleFileUpload);
    server.on("/direct_alert", HTTP_GET, handleDirectAlert);
    server.on("/reset_demo", HTTP_GET, handleResetDemo);

    server.begin();
    Serial.println("SERVEUR WEB DEMARRE");
}

void loop() {
    server.handleClient();
    delay(2);
}
