# Flasheur WiFi pour RP2040 via ESP32

Ce projet transforme un ESP32 en un pont WiFi pour téléverser ("flasher") des firmwares sur un microcontrôleur RP2040 à distance, sans aucune connexion physique à un ordinateur. Tout est géré via une interface web simple et intuitive.

## ✨ Fonctionnalités

* **Flashage sans fil** : Mettez à jour votre RP2040 via WiFi.
* **Interface Web Intuitive** : Une page web simple pour gérer tout le processus.
* **Glisser-Déposer** : Téléversez vos fichiers `.bin` facilement.
* **Suivi en Temps Réel** : Barres de progression pour le téléversement et les étapes de flashage (effacement, écriture).
* **Console de Statut** : Suivez les logs détaillés directement depuis l'interface.
* **Mode Point d'Accès** : L'ESP32 crée son propre réseau WiFi pour une utilisation sur le terrain.

---

## ⚠️ Prérequis Indispensable

Pour que ce flasheur fonctionne, le RP2040 **doit** avoir été flashé au préalable avec un bootloader spécifique qui sait communiquer avec l'ESP32.

* **Bootloader requis :** [TheMalkavien/rp2040-serial-bootloader](https://github.com/TheMalkavien/rp2040-serial-bootloader)

Veuillez suivre les instructions de ce dépôt pour installer le bootloader sur votre RP2040 avant de tenter d'utiliser ce flasheur WiFi. Sans ce bootloader, l'ESP32 ne pourra pas synchroniser et flasher le firmware.

---

## 🔌 Guide de Branchement : ESP32 vers RP2040

Pour que l'ESP32 puisse communiquer et contrôler le RP2040, des connexions physiques précises sont nécessaires. Assurez-vous que les deux cartes partagent une masse commune (GND).

| Broche ESP32 | Rôle | Vers la broche RP2040 |
| :--- | :--- | :--- |
| **GND** | Masse | **GND** |
| **GPIO 2** | Contrôle du Reset | **RESET** |
| **GPIO 3** | Contrôle du mode Bootloader| **GPIO 22** |
| **GPIO 7 (TX)** | Communication (Transmission) | **GPIO 8 (RX)** |
| **GPIO 8 (RX)** | Communication (Réception) | **GPIO 9 (TX)** |

> **Note importante :** La communication série est croisée. Le transmetteur (TX) de l'ESP32 doit être connecté au récepteur (RX) du RP2040, et vice-versa.

---

## 🚀 Guide d'Utilisation : Flasher le Firmware

Une fois le bootloader requis installé et les branchements effectués, suivez ces étapes pour téléverser un nouveau firmware.

### Étape 1 : Connexion au Flasheur

1.  **Mettez sous tension** votre montage ESP32/RP2040.
2.  Sur votre ordinateur ou smartphone, recherchez les réseaux Wi-Fi et connectez-vous au point d'accès créé par l'ESP32.
    * **Nom du réseau (SSID) :** `ESP32-Uploader`
    * **Mot de passe :** `12345678`
3.  Ouvrez un navigateur web et rendez-vous à l'adresse `http://192.168.4.1`. Vous devriez voir l'interface du flasheur.

### Étape 2 : Processus de Flashage

1.  **Téléverser le firmware :**
    * Cliquez sur la zone "Choisir ou glisser un fichier".
    * Sélectionnez le fichier de firmware `.bin` que vous souhaitez installer.
    * Cliquez sur le bouton **"1. Téléverser"**. Une barre de progression verte indiquera l'avancement.
2.  **Préparer le RP2040 :**
    * Une fois le téléversement terminé, le bouton **"2. Préparer le Flash"** devient cliquable.
    * Cliquez dessus. L'ESP32 va mettre le RP2040 en mode bootloader et tenter de s'y synchroniser.
3.  **Démarrer le Flashage :**
    * Après une synchronisation réussie, une nouvelle section apparaît avec le bouton **"3. Démarrer le Flash"**.
    * Cliquez sur ce bouton pour lancer l'opération. Les boutons seront désactivés pendant le processus.
    * Une barre de progression bleue s'affichera, avec l'état actuel ("Effacement en cours...", "Flashage en cours...").
4.  **Fin du processus :**
    * Une fois le flashage terminé à 100%, un message de succès s'affichera.
    * Le RP2040 redémarrera automatiquement avec le nouveau firmware. L'interface web se rechargera après quelques secondes, prête pour une nouvelle opération.

Vous pouvez suivre toutes ces étapes et les messages de débogage dans la console en bas de la page.
