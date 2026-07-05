// Configuration initiale dans setup()
void setup() {
  // Initialisation du port série
  Serial.begin(115200);

  // Initialisation du système de fichiers
  if (!LittleFS.begin()) {
    Serial.println("Erreur initialisation LittleFS");
    return;
  }

  // Chargement de la configuration
  ConfigStore.load();

  // Initialisation de l'affichage
  DisplayManager.begin();

  // Initialisation des relais
  RelayController.begin();

  // Initialisation du contrôleur de température
  TemperatureController.begin(&relays);

  // Configuration WiFi avec WiFiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("FermCon");

  // Initialisation du serveur iSpindel
  ISpindelReceiver.begin();

  // Initialisation du publisher de données
  DataPublisher.begin();

  // Initialisation du serveur web
  WebServerManager.begin();

  // Configuration OTA
  ArduinoOTA.begin();

  // Chargement du profil de fermentation
  ConfigStore.loadProfile(profile);
}

// Boucle principale non bloquante
void loop() {
  // Mise à jour prioritaire du contrôleur de température
  TemperatureController.update();

  // Calcul de la consigne
  float setpoint = profile.isActive() ? profile.getCurrentSetpoint() : FIXED_SETPOINT;
  tempCtrl.setSetpoint(setpoint);

  // Mise à jour de l'affichage toutes les 500ms
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 500) {
    DisplayData data = {
      tempCtrl.getCurrentTemp(),
      setpoint,
      WiFi.localIP().toString(),
      gravity,
      tempCtrl.isCoolOn(),
      tempCtrl.isHeatOn(),
      profile.getCurrentStepInfo(),
      tempCtrl.isFault()
    };
    DisplayManager.update(data);
    lastDisplayUpdate = millis();
  }

  // Publication des données (non bloquant)
  DataPublisher.publish();

  // Gestion de la reconnexion WiFi
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }

  // Mode test iSpindel (simulation)
#ifdef WOKWI_SIM
  static unsigned long lastSimulatedSpindel = 0;
  if (millis() - lastSimulatedSpindel >= 15000) {
    String simulatedData = "{\"name\":\"Simulated iSpindel\",\"ID\":1,\"temperature\":21.5,\"temp_units\":\"C\",\"gravity\":1.050,\"angle\":45,\"battery\":3.7,\"RSSI\":-60}";
    ISpindelReceiver.processData(simulatedData);
    lastSimulatedSpindel = millis();
  }
#endif
}

/*
Pour activer le mode test iSpindel (WOKWI_SIM) :
1. Ajouter dans platformio.ini :
   build_flags = -DWOKWI_SIM
2. Le mode injectera une trame iSpindel simulée toutes les 15 secondes
3. La trame contient des valeurs typiques pour tester l'affichage et la redistribution
*/
