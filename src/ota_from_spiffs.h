#pragma once
#include <Arduino.h>
#include <functional>

void printOtaInfo();

/**
 * @brief Applique une mise à jour OTA à partir d'un fichier stocké sur LittleFS/SPIFFS.
 *
 * @param path                 Chemin du binaire (ex: "/firmware.bin").
 * @param progress_cb          Callback de progression: (pct, msg). pct ∈ [0..100]. msg optionnel (peut être nullptr).
 * @param delete_after_success Supprimer le fichier après succès (false par défaut).
 * @return true                Si la procédure s'est déroulée correctement (en pratique l'app redémarre).
 * @return false               En cas d'erreur; le callback reçoit un message d'erreur.
 *
 * Comportement:
 *  - Ouvre path, vérifie la taille minimale (~64 KiB).
 *  - Update.begin(total_size) sur la partition OTA disponible.
 *  - Stream le contenu par blocs (4 KiB) avec progression.
 *  - Update.end(true) => sélectionne la nouvelle partition pour le boot.
 *  - ESP.restart().
 */
bool ota_apply_from_spiffs(const char* path,
                           std::function<void(int, const char*)> progress_cb = nullptr,
                           bool delete_after_success = false);

/**
 * @brief Valide l'image OTA au boot si on est en "PENDING_VERIFY".
 * À appeler très tôt dans setup() après init minimal des logs.
 *
 * @param on_check Fonction utilisateur qui retourne true si le self-test est OK.
 *                 Exemple: [](){ return true; } ou une lambda qui vérifie quelques invariants.
 */
void ota_validate_running_image(std::function<bool(void)> on_check);

// ================== OTA asynchrone (tâche FreeRTOS) ==================
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Callback de progression: pct ∈ [0..100], msg optionnel (nullptr si rien à dire)
using OtaProgressCb = std::function<void(int, const char*)>;

// Arguments passés à la tâche
struct OtaTaskArgs {
  String        path;                  // ex: "/firmware.bin"
  OtaProgressCb user_cb;               // callback utilisateur (log, WS, BLE…)
  bool          delete_after_success;  // supprimer le fichier après succès
};

// Lance l’OTA dans une tâche dédiée (retourne pdPASS si ok)
BaseType_t ota_start_task(const char* path,
                          OtaProgressCb cb,
                          bool delete_after_success = false,
                          const char* task_name = "ota_task",
                          uint32_t stack_words = 8192,
                          UBaseType_t priority = 1,
                          BaseType_t core = 1);