### English Version

# ESP32-S3 BIN Wireless Uploader

This project is a web-based wireless flasher for a Raspberry Pi RP2040, running on an ESP32-S3 microcontroller. It allows you to wirelessly upload a `.bin` firmware file to a connected RP2040 device via a web interface. The ESP32-S3 acts as a Wi-Fi access point, a web server, and a bootloader interface for the RP2040, communicating via a non-blocking state machine.

## Features

  - **Wireless Firmware Upload**: Upload `.bin` files to the RP2040 without a physical USB connection.
  - **Web Interface**: A simple and responsive web page to select and upload the firmware.
  - **Access Point Mode**: The ESP32-S3 creates its own Wi-Fi network for easy connection.
  - **Inactivity Deep Sleep**: The ESP32-S3 enters deep sleep mode after a period of inactivity to save power. It can be woken up by a dedicated pin.
  - **LittleFS**: The web interface files are stored on the ESP32's LittleFS filesystem.
  - **Non-blocking Flasher State Machine**: The RP2040 flashing process is handled by a non-blocking state machine, allowing the ESP32 to continue serving the web interface and managing other tasks.

## Hardware Requirements

  - **ESP32-S3-DevKitC-1** (or a similar ESP32-S3 board)
  - A **Raspberry Pi RP2040** based board
  - **Connections**:
      - `RESETRP2040_PIN` (GPIO2) on ESP32 connected to the `RUN` pin of the RP2040.
      - `BOOTLOADER_PIN` (GPIO3) on ESP32 connected to the `BOOTSEL` pin of the RP2040 (requires a level shifter if voltages don't match, e.g., 3.3V to 3.3V).
      - `RP2040_SERIAL_TX_PIN` (GPIO7) on ESP32 connected to `RP2040 RX`
      - `RP2040_SERIAL_RX_PIN` (GPIO8) on ESP32 connected to `RP2040 TX`
      - `WAKEUP_PIN` (GPIO1) on ESP32 connected to a button or switch to wake the device from deep sleep.

## Software Requirements

  - **PlatformIO**: The project is configured to be built with PlatformIO.
  - **Libraries**: The `platformio.ini` file lists the required libraries. They will be automatically installed by PlatformIO.
      - `littlefs`
      - `ESPAsyncWebServer`
      - `AsyncTCP`

## Configuration

You can customize the project settings in the `config.h` file:

  - **Wi-Fi SSID and Password**:
    ```c++
    #define SSID "ESP32-Uploader"
    #define PASSWORD "12345678"
    ```
  - **Pin Definitions**:
    ```c++
    #define WAKEUP_PIN GPIO_NUM_1
    #define RESETRP2040_PIN GPIO_NUM_2
    #define BOOTLOADER_PIN GPIO_NUM_3
    #define RP2040_SERIAL_TX_PIN 7
    #define RP2040_SERIAL_RX_PIN 8
    ```
  - **Inactivity Timeout**: The time in milliseconds before the ESP32 enters deep sleep.
    ```c++
    #define INACTIVITY_TIMEOUT (10 * 60 * 1000) // 10 minutes
    ```

## Usage

1.  **Build and Upload the Project**:

      - Open the project in your PlatformIO IDE.
      - Connect the ESP32-S3 board to your computer.
      - Upload the code and the LittleFS filesystem content (the `data` directory, which contains `index.html`) to the ESP32. In PlatformIO, you can do this by running the "PlatformIO: Upload Filesystem Image" task.
      - *Note*: For full consistency, you may want to update the `<title>` tag in `index.html` from "ESP32 UF2 Uploader" to "ESP32 BIN Uploader".

2.  **Connect to the ESP32**:

      - Power up the ESP32-S3 board.
      - Connect your computer or smartphone to the Wi-Fi network named `ESP32-Uploader` (or the custom SSID you defined) using the password `12345678` (or your custom password).

3.  **Access the Web Interface**:

      - Open a web browser and navigate to the IP address of the ESP32 (usually `192.168.4.1`).
      - The `index.html` page will be displayed.

4.  **Flash the RP2040**:

      - On the web page, click the button to put the RP2040 in bootloader mode. This will use the ESP32's GPIO pins to trigger the reset sequence.
      - Select your `.bin` firmware file using the file selector.
      - Click the "Start Flash" button. The progress will be displayed on the page.
      - Once the flashing is complete, the RP2040 will reboot with the new firmware.

-----

### Version Française

# ESP32-S3 BIN Wireless Uploader

Ce projet est un flasheur web sans fil pour un Raspberry Pi RP2040, fonctionnant sur un microcontrôleur ESP32-S3. Il vous permet de téléverser sans fil un fichier de firmware `.bin` vers un RP2040 connecté via une interface web. L'ESP32-S3 agit comme un point d'accès Wi-Fi, un serveur web et une interface de bootloader pour le RP2040, communiquant via une machine à états non bloquante.

## Fonctionnalités

  - **Téléversement de Firmware sans Fil** : Téléchargez des fichiers `.bin` vers le RP2040 sans connexion USB physique.
  - **Interface Web** : Une page web simple et réactive pour sélectionner et téléverser le firmware.
  - **Mode Point d'Accès** : L'ESP32-S3 crée son propre réseau Wi-Fi pour une connexion facile.
  - **Mise en Veille Profonde après Inactivité** : L'ESP32-S3 entre en mode veille profonde après une période d'inactivité pour économiser de l'énergie. Il peut être réveillé par une broche dédiée.
  - **LittleFS** : Les fichiers de l'interface web sont stockés sur le système de fichiers LittleFS de l'ESP32.
  - **Machine à États de Flashage Non Bloquante** : Le processus de flashage du RP2040 est géré par une machine à états non bloquante, permettant à l'ESP32 de continuer à servir l'interface web et de gérer d'autres tâches.

## Matériel Requis

  - **ESP32-S3-DevKitC-1** (ou une carte ESP32-S3 similaire)
  - Une carte basée sur **Raspberry Pi RP2040**
  - **Connexions** :
      - `RESETRP2040_PIN` (GPIO2) de l'ESP32 connecté à la broche `RUN` du RP2040.
      - `BOOTLOADER_PIN` (GPIO3) de l'ESP32 connecté à la broche `BOOTSEL` du RP2040 (nécessite un convertisseur de niveau si les tensions ne correspondent pas, par exemple, 3.3V à 3.3V).
      - `RP2040_SERIAL_TX_PIN` (GPIO7) de l'ESP32 connecté au `RX` du RP2040.
      - `RP2040_SERIAL_RX_PIN` (GPIO8) de l'ESP32 connecté au `TX` du RP2040.
      - `WAKEUP_PIN` (GPIO1) de l'ESP32 connecté à un bouton ou un interrupteur pour réveiller l'appareil de la veille profonde.

## Logiciels Requis

  - **PlatformIO** : Le projet est configuré pour être compilé avec PlatformIO.
  - **Bibliothèques** : Le fichier `platformio.ini` liste les bibliothèques requises. Elles seront installées automatiquement par PlatformIO.
      - `littlefs`
      - `ESPAsyncWebServer`
      - `AsyncTCP`

## Configuration

Vous pouvez personnaliser les paramètres du projet dans le fichier `config.h` :

  - **SSID et Mot de Passe Wi-Fi** :
    ```c++
    #define SSID "ESP32-Uploader"
    #define PASSWORD "12345678"
    ```
  - **Définitions des Broches** :
    ```c++
    #define WAKEUP_PIN GPIO_NUM_1
    #define RESETRP2040_PIN GPIO_NUM_2
    #define BOOTLOADER_PIN GPIO_NUM_3
    #define RP2040_SERIAL_TX_PIN 7
    #define RP2040_SERIAL_RX_PIN 8
    ```
  - **Délai d'Inactivité** : Le temps en millisecondes avant que l'ESP32 n'entre en veille profonde.
    ```c++
    #define INACTIVITY_TIMEOUT (10 * 60 * 1000) // 10 minutes
    ```

## Utilisation

1.  **Compiler et Téléverser le Projet** :

      - Ouvrez le projet dans votre IDE PlatformIO.
      - Connectez la carte ESP32-S3 à votre ordinateur.
      - Téléversez le code et le contenu du système de fichiers LittleFS (le dossier `data`, qui contient `index.html`) sur l'ESP32. Dans PlatformIO, vous pouvez le faire en exécutant la tâche "PlatformIO: Upload Filesystem Image".
      - *Note* : Pour une cohérence totale, vous pouvez mettre à jour la balise `<title>` dans `index.html` de "ESP32 UF2 Uploader" à "ESP32 BIN Uploader".

2.  **Se Connecter à l'ESP32** :

      - Allumez la carte ESP32-S3.
      - Connectez votre ordinateur ou smartphone au réseau Wi-Fi nommé `ESP32-Uploader` (ou le SSID personnalisé que vous avez défini) en utilisant le mot de passe `12345678` (ou votre mot de passe personnalisé).

3.  **Accéder à l'Interface Web** :

      - Ouvrez un navigateur web et naviguez vers l'adresse IP de l'ESP32 (généralement `192.168.4.1`).
      - La page `index.html` s'affichera.

4.  **Flasher le RP2040** :

      - Sur la page web, cliquez sur le bouton pour mettre le RP2040 en mode bootloader. Cela utilisera les broches GPIO de l'ESP32 pour déclencher la séquence de réinitialisation.
      - Sélectionnez votre fichier de firmware `.bin` à l'aide du sélecteur de fichiers.
      - Cliquez sur le bouton "Start Flash". La progression s'affichera sur la page.
      - Une fois le flashage terminé, le RP2040 redémarrera avec le nouveau firmware.
