# Flasheur WiFi & Bluetooth pour RP2040 via ESP32

Ce projet transforme un ESP32 en un pont **WiFi et Bluetooth** pour téléverser ("flasher") des firmwares sur un microcontrôleur RP2040 à distance, sans aucune connexion physique à un ordinateur.  
La gestion se fait via une **interface web moderne et intuitive**, accessible aussi bien en WiFi qu’en Bluetooth.

---

## ✨ Fonctionnalités

* **Flashage sans fil** : Mettez à jour votre RP2040 via **WiFi ou Bluetooth**.  
* **Nouvelle Interface Web** : Interface repensée, plus claire et réactive.  
* **Glisser-Déposer** : Téléversez vos fichiers `.bin` simplement.  
* **Suivi en Temps Réel** : Barres de progression pour l’upload et les étapes de flashage (effacement, écriture).  
* **Console de Statut** : Logs détaillés directement depuis l’interface.  
* **Console Série** : Page `/serial.html` pour lire et écrire sur l’UART du RP2040 depuis le navigateur, en parallèle du pont TCP sur le port `4403` (`nc`, `telnet`, PuTTY…).  
* **Mode Point d’Accès WiFi** : L’ESP32 peut créer son propre réseau WiFi pour une utilisation sur le terrain.  
* **Connexion Bluetooth** : Utilisation simplifiée depuis un smartphone, sans réseau WiFi nécessaire.  

---

## ⚠️ Prérequis Indispensable

Pour que ce flasheur fonctionne, le RP2040 **doit** avoir été flashé au préalable avec un bootloader compatible avec la communication série via l’ESP32.  

* **Bootloader requis :** [TheMalkavien/rp2040-serial-bootloader](https://github.com/TheMalkavien/rp2040-serial-bootloader)  

---

## 🔌 Guide de Branchement : ESP32 vers RP2040

Assurez-vous que les deux cartes partagent une masse commune (GND).

| Broche ESP32S3-zero | Broche ESP32S3-xiao seedstudio | Rôle                          | Vers la broche RP2040 |
|---------------------|--------------------------------|-------------------------------|------------------------|
| **GPIO 2 (PIN 2)**          | **GPIO 2 (A1/D1)**                     | Contrôle du Reset (optionnel) | **RUN**              |
| **GPIO 3 (PIN 3)**          | **GPIO 3 (A2/D2)**                     | Contrôle du mode Bootloader   | **GPIO 22 / PIN22**            |
| **GPIO 7 (TX)**     | **GPIO 43 (D6) (TX)**               | Communication (Transmission)  | **GPIO 9 (RX)**        |
| **GPIO 8 (RX)**     | **GPIO 44 (D7) (RX)**               | Communication (Réception)     | **GPIO 8 (TX)**        |

---

## 🚀 Guide d’Utilisation : Flasher le Firmware

### Étape 1 : Connexion au Flasheur

**WiFi**  
1. Allumez l’ESP32/RP2040.  
2. Connectez votre appareil au réseau :  
   * **SSID :** `ESP32-Uploader`  
   * **Mot de passe :** `12345678`  
3. Accédez à l’interface via : `http://192.168.4.1`  

**Bluetooth**  
1. Activez le Bluetooth.  
2. Associez l’appareil **ESP32-Uploader**.  
3. Utilisez un navigateur/app compatible WebSerial.  
4. Accédez à l’interface via : [https://themalkavien.github.io/RP2040_WIFI_OTA/data/index.html](https://themalkavien.github.io/RP2040_WIFI_BLE_OTA/data/index.html)

---

### Étape 2 : Processus de Flashage

1. **Téléverser le firmware** → Choisissez un `.bin`, cliquez sur **Téléverser**.  
2. **Préparer le RP2040** → bouton **Préparer le Flash**.  
3. **Démarrer le Flashage** → bouton **Démarrer le Flash**.  
4. **Fin** → succès affiché, redémarrage auto du RP2040.  

---

## 📦 Exemple d’Utilisation

- Mise à jour via smartphone en Bluetooth.  
- Flashage WiFi sans câble en atelier.  

---

## 📜 Licence

MIT – libre à vous de l’utiliser, modifier et améliorer.

---

# WiFi & Bluetooth Flasher for RP2040 via ESP32

This project turns an ESP32 into a **WiFi and Bluetooth bridge** to remotely flash firmware on an RP2040 microcontroller, without any physical connection to a computer.  
Management is done through a **modern and intuitive web interface**, available over both WiFi and Bluetooth.

---

## ✨ Features

* **Wireless flashing**: Update your RP2040 over **WiFi or Bluetooth**.  
* **New Web Interface**: Cleaner, more responsive design.  
* **Drag & Drop**: Upload your `.bin` files easily.  
* **Real-Time Progress**: Progress bars for upload, erase, and write steps.  
* **Status Console**: Detailed logs directly in the interface.  
* **Serial Console**: `/serial.html` page to read from and write to the RP2040 UART from the browser, alongside the TCP bridge on port `4403` (`nc`, `telnet`, PuTTY…).  
* **WiFi Access Point Mode**: ESP32 creates its own network for offline use.  
* **Bluetooth Connection**: Easy flashing from a smartphone without WiFi.  

---

## ⚠️ Requirements

The RP2040 **must** be pre-flashed with a specific bootloader compatible with ESP32 serial communication.  

* **Required bootloader:** [TheMalkavien/rp2040-serial-bootloader](https://github.com/TheMalkavien/rp2040-serial-bootloader)  

---

## 🔌 Wiring Guide: ESP32 to RP2040

Make sure both boards share a common ground (GND).

| ESP32S3-zero Pin    | ESP32S3-xiao seedstudio Pin | Function                     | RP2040 Pin |
|---------------------|-----------------------------|------------------------------|------------|
| **GPIO 2 (PIN 2)**          | **GPIO 2 (PIN A1/D1)**                  | Reset Control (optional)     | **RUN**  |
| **GPIO 3 (PIN 3)**          | **GPIO 3 (PIN A2/D2)**                  | Bootloader Mode Control      | **GPIO 22 / PIN22**|
| **GPIO 7 (TX)**     | **GPIO 43 (D6) (TX)**            | Communication (Transmit)     | **GPIO 9 (RX)** |
| **GPIO 8 (RX)**     | **GPIO 44 (D7) (RX)**            | Communication (Receive)      | **GPIO 8 (TX)** |

---

## 🚀 Usage Guide: Flashing Firmware

### Step 1: Connect to the Flasher

**WiFi**  
1. Power on ESP32/RP2040.  
2. Connect your device to:  
   * **SSID:** `ESP32-Uploader`  
   * **Password:** `12345678`  
3. Open browser at: `http://192.168.4.1`  

**Bluetooth**  
1. Enable Bluetooth.  
2. Pair with **ESP32-Uploader**.  
3. Use a WebSerial-compatible browser/app.  
4. Access the flasher interface at: [https://themalkavien.github.io/RP2040_WIFI_OTA/data/index.html](https://themalkavien.github.io/RP2040_WIFI_BLE_OTA/data/index.html)

---

### Step 2: Flashing Process

1. **Upload firmware** → choose `.bin`, click **Upload**.  
2. **Prepare RP2040** → click **Prepare Flash**.  
3. **Start Flashing** → click **Start Flash**.  
4. **Finish** → success message, RP2040 auto-reboots.  

---

## 📦 Example Use Cases

- Quick updates in the field via Bluetooth on a phone.  
- Workshop WiFi flashing without plugging boards in.  

---

## 📜 License

MIT – free to use, modify, and improve.
