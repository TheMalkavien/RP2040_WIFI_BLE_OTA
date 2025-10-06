#include "ota_from_spiffs.h"
#include <FS.h>
#include <LittleFS.h>
#include <Update.h>
extern "C" {
  #include "esp_ota_ops.h"
}
#include "esp_ota_ops.h"
#include "esp_app_format.h"

static const char* ota_state_str(esp_ota_img_states_t s){
  switch(s){
    case ESP_OTA_IMG_NEW:             return "NEW";
    case ESP_OTA_IMG_PENDING_VERIFY:  return "PENDING_VERIFY";
    case ESP_OTA_IMG_VALID:           return "VALID";
    case ESP_OTA_IMG_INVALID:         return "INVALID";
    case ESP_OTA_IMG_ABORTED:         return "ABORTED";
    case ESP_OTA_IMG_UNDEFINED:       return "UNDEFINED";
    default:                          return "?";
  }
}

void printOtaInfo(){
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* boot    = esp_ota_get_boot_partition();
  const esp_partition_t* next    = esp_ota_get_next_update_partition(NULL);

  esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
  if (running) esp_ota_get_state_partition(running, &st);

  esp_app_desc_t desc{};
  const esp_partition_t* active = running ? running : boot;
  if (active) {
    if (esp_ota_get_partition_description(active, &desc) == ESP_OK) {
      // ok
    }
  }

  Serial.println("=== OTA/Partitions ===");
  if (running)  Serial.printf("Running : %s @ 0x%06x (%u KiB)\n", running->label, running->address, running->size/1024);
  if (boot)     Serial.printf("Boot    : %s @ 0x%06x\n",          boot->label,    boot->address);
  if (next)     Serial.printf("Next OTA: %s @ 0x%06x (%u KiB)\n", next->label,    next->address, next->size/1024);
  Serial.printf("State   : %s\n", ota_state_str(st));
  if (active) {
    Serial.printf("Version : %.*s\n", sizeof desc.version, desc.version);
    Serial.printf("Build   : %.*s %.*s\n", sizeof desc.date, desc.date, sizeof desc.time, desc.time);
  }
  Serial.println("======================");
}

static inline void tick(std::function<void(int,const char*)>& cb, int pct, const char* m = nullptr) {
  if (cb) cb(pct, m);
}

bool ota_apply_from_spiffs(const char* path,
                           std::function<void(int, const char*)> progress_cb,
                           bool delete_after_success) {
  if (!path || !*path) { tick(progress_cb, 0, "OTA: chemin invalide"); return false; }

  // S'assurer que FS est monté; si déjà monté, begin() renverra true quand même.
  if (!LittleFS.begin()) {
    tick(progress_cb, 0, "OTA: LittleFS.begin() a échoué");
    return false;
  }

  if (!LittleFS.exists(path)) {
    tick(progress_cb, 0, "OTA: fichier introuvable");
    return false;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    tick(progress_cb, 0, "OTA: ouverture impossible");
    return false;
  }

  const size_t total = f.size();
  if (total < (64u * 1024u)) {
    f.close();
    tick(progress_cb, 0, "OTA: fichier trop petit pour être valide");
    return false;
  }

  // Initialise l'opération OTA sur la prochaine partition libre
  if (!Update.begin(total)) {
    const char* err = Update.errorString();
    tick(progress_cb, 0, err && *err ? err : "OTA: Update.begin() a échoué");
    f.close();
    return false;
  }

  // Buffer de copie (4 KiB)
  const size_t BUF_SZ = 4096;
  std::unique_ptr<uint8_t[]> buf(new (std::nothrow) uint8_t[BUF_SZ]);
  if (!buf) {
    Update.abort();
    f.close();
    tick(progress_cb, 0, "OTA: allocation buffer 4K impossible");
    return false;
  }

  size_t written = 0;
  int lastPct = -1;
  tick(progress_cb, 0, "OTA: démarrage…");

  while (f.available()) {
    size_t n = f.read(buf.get(), BUF_SZ);
    if (n == 0) break;

    size_t w = Update.write(buf.get(), n);
    if (w != n) {
      const char* err = Update.errorString();
      tick(progress_cb, (lastPct >= 0 ? lastPct : 0), err && *err ? err : "OTA: écriture partielle");
      Update.abort();
      f.close();
      return false;
    }

    written += n;
    if ((written & 0x3FFF) == 0) {     // toutes les 16 KiB
        // Donne du temps aux stacks réseau et au WDT
        vTaskDelay(1);    // 1 tick

    }
    int pct = static_cast<int>((written * 100ull) / total);
    if (pct != lastPct) {
      lastPct = pct;
      tick(progress_cb, pct, nullptr);
    }
  }

  f.close();

  if (!Update.end(true)) {  // true => select new partition for boot
    const char* err = Update.errorString();
    tick(progress_cb, (lastPct >= 0 ? lastPct : 0), err && *err ? err : "OTA: fin échouée");
    return false;
  }

  tick(progress_cb, 100, "OTA: terminé, redémarrage…");

  if (delete_after_success) {
    LittleFS.remove(path);
  }

  delay(150);
  ESP.restart();
  return true;  // on ne revient normalement pas ici
}

void ota_validate_running_image(std::function<bool(void)> on_check) {
  esp_ota_img_states_t state;
  const esp_partition_t* running = esp_ota_get_running_partition();

  if (!running) return;
  if (esp_ota_get_state_partition(running, &state) != ESP_OK) return;

  if (state == ESP_OTA_IMG_PENDING_VERIFY) {
    bool ok = true;
    if (on_check) {
      // Entourer d'un try-catch si exceptions activées (par défaut non en Arduino)
      ok = on_check();
    }
    if (ok) {
      esp_ota_mark_app_valid_cancel_rollback();
    } else {
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }
  }
}

// Anti double-launch (optionnel, global fichier)
static volatile bool s_otaTaskRunning = false;

// Tâche: applique l’OTA avec un throttle de logs (+5% ou 250 ms)
static void ota_task(void* pv) {
  std::unique_ptr<OtaTaskArgs> args(static_cast<OtaTaskArgs*>(pv));

  // Wrapper de throttle autour du callback utilisateur
  uint32_t lastMs  = 0;
  int      lastPct = -1;

  auto throttled_cb = [&](int pct, const char* msg) {
    const uint32_t now = millis();
    const bool allow =
        (msg && *msg)            // message explicite
        || pct == 100            // fin
        || lastPct < 0           // premier tick
        || (pct - lastPct) >= 5  // +5% mini
        || (now - lastMs) >= 250;// ou 250 ms écoulées

    if (!allow) return;

    lastMs  = now;
    lastPct = pct;

    if (args->user_cb) {
      args->user_cb(pct, msg);
    }
  };

  // Petit message d’entrée
  throttled_cb(0, "OTA: démarrage…");

  // Exécute l’OTA synchrone (qui redémarre à la fin si succès)
  bool ok = ota_apply_from_spiffs(args->path.c_str(), throttled_cb, args->delete_after_success);

  // Si on revient ici, c’est qu’il y a eu un échec (ou pas de reboot)
  if (!ok) {
    if (args->user_cb) args->user_cb(lastPct >= 0 ? lastPct : 0, "OTA: échec.");
  }

  s_otaTaskRunning = false;
  vTaskDelete(nullptr);
}

// Lanceur de tâche
BaseType_t ota_start_task(const char* path,
                          OtaProgressCb cb,
                          bool delete_after_success,
                          const char* task_name,
                          uint32_t stack_words,
                          UBaseType_t priority,
                          BaseType_t core) {
  if (s_otaTaskRunning) {
    // Déjà en cours; signale via callback si fourni
    if (cb) cb(0, "OTA: une mise à jour est déjà en cours.");
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY; // retourne un truc non-pdPASS
  }

  // Copie des arguments pour la tâche
  auto* args = new (std::nothrow) OtaTaskArgs{
      String(path ? path : "/firmware.bin"),
      std::move(cb),
      delete_after_success
  };
  if (!args) return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

  s_otaTaskRunning = true;
  BaseType_t ok = xTaskCreatePinnedToCore(
      ota_task,
      task_name,
      stack_words,
      args,
      priority,
      nullptr,
      core
  );

  if (ok != pdPASS) {
    s_otaTaskRunning = false;
    delete args;
  }
  return ok;
}