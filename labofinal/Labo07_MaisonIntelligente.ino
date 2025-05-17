#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <LedControl.h>
#include "Alarm.h"
#include "ViseurAutomatique.h"
#include <WiFiEspAT.h>
#include <PubSubClient.h>

#if HAS_SECRETS
#include "arduino_secrets.h"
const char ssid[] = SECRET_SSID;
const char pass[] = SECRET_PASS;
#else
const char ssid[] = "TechniquesInformatique-Etudiant";
const char pass[] = "shawi123";
#endif

WiFiClient espClient;
PubSubClient Client(espClient);
char mqttTopic[] = "etd/30/data";

unsigned long lastMqttSend1 = 0;
unsigned long lastMqttSend2 = 0;
const unsigned long MQTT_SEND_INTERVAL1 = 2500;
const unsigned long MQTT_SEND_INTERVAL2 = 1100;
unsigned long lastMqttAttempt = 0;

const char* NUM_ETUDIANT = "2413335";
LiquidCrystal_I2C lcd(0x27, 16, 2);
unsigned long lastLcdUpdate = 0;
const unsigned long lcdUpdateInterval = 500;

bool motorEnabled = false;
const int potPin = A0;
int potValue = 0;  // Valeur lue (0–1023)
int potPourcent = 0;


LedControl matrix = LedControl(42, 44, 46, 1);
unsigned long symboleExpire = 0;
bool symboleActif = false;
byte* symboleActuel = nullptr;

byte symboleCheck[8] = {
  B00000000, B00000001, B00000010, B00000100,
  B10001000, B01010000, B00100000, B00000000
};

byte symboleX[8] = {
  B10000001, B01000010, B00100100, B00011000,
  B00011000, B00100100, B01000010, B10000001
};

byte symboleInterdit[8] = {
  B00111100, B01000010, B10100001, B10010001,
  B10001001, B10000101, B01000010, B00111100
};

const int trigPin = 6;
const int echoPin = 7;
float distance = 0;
unsigned long lastDistanceRead = 0;
const unsigned long distanceInterval = 50;
String lcdLine1 = "";
String lcdLine2 = "";

unsigned long lastSerialTime = 0;
const unsigned long serialInterval = 100;

Alarm alarme(9, 8, 10, 2, &distance);
ViseurAutomatique viseur(31, 33, 35, 37, distance);
unsigned long _currentTime = 0;

int limiteAlarme = 15;
int limInf = 30;
int limSup = 60;

void initialiserLCD() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(NUM_ETUDIANT);
  lcd.setCursor(0, 1);
  lcd.print("Labo 7");
  delay(2000);
  lcd.clear();
}

void initialiserMatrix() {
  matrix.shutdown(0, false);
  matrix.setIntensity(0, 5);
  matrix.clearDisplay(0);
}

void afficherSymbole(byte symbole[8]) {
  symboleActif = true;
  symboleActuel = symbole;
  symboleExpire = millis() + 3000;
  for (int i = 0; i < 8; i++) matrix.setRow(0, i, symbole[i]);
}

void tacheEffacerSymbole() {
  if (symboleActif && millis() > symboleExpire) {
    for (int i = 0; i < 8; i++) matrix.setRow(0, i, 0);
    symboleActif = false;
  }
}

long lireDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duree = pulseIn(echoPin, HIGH, 30000);
  long d = duree * 0.034 / 2;
  if (d < 2 || d > 400) return distance;
  return d;
}

void tacheLireDistance() {
  if (millis() - lastDistanceRead >= distanceInterval) {
    distance = lireDistanceCM();
    lastDistanceRead = millis();
  }
}

void tacheAffichageLCD() {
  if (millis() - lastLcdUpdate >= lcdUpdateInterval) {
    lcdLine1 = "Dist: " + String(distance, 0) + " cm";


    if (distance < limInf) {
      lcdLine2 = "Trop pres";
    } else if (distance > limSup) {
      lcdLine2 = "Trop loin";
    } else {
      lcdLine2 = "Angle: " + String(viseur.getAngle(), 0);
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(lcdLine1);
    lcd.setCursor(0, 1);
    lcd.print(lcdLine2);

    lastLcdUpdate = millis();
  }
}

void tacheCommande() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    bool cmdOK = false;
    bool erreurLimites = false;

    if (cmd.equalsIgnoreCase("g_dist")) {
      long mesure = lireDistanceCM();
      Serial.println(mesure);
      afficherSymbole(symboleCheck);
      return;
    } else if (cmd.startsWith("cfg;alm;")) {
      int val = cmd.substring(8).toInt();
      if (val > 0) {
        alarme.setDistance(val);
        cmdOK = true;
      }
    } else if (cmd.startsWith("cfg;lim_inf;")) {
      int val = cmd.substring(12).toInt();
      if (val < limSup) {
        viseur.setDistanceMinSuivi(val);
        cmdOK = true;
      } else erreurLimites = true;
    } else if (cmd.startsWith("cfg;lim_sup;")) {
      int val = cmd.substring(12).toInt();
      if (val > limInf) {
        viseur.setDistanceMaxSuivi(val);
        cmdOK = true;
      } else erreurLimites = true;
    }

    if (erreurLimites) afficherSymbole(symboleInterdit);
    else if (!cmdOK) afficherSymbole(symboleX);
    else afficherSymbole(symboleCheck);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  initialiserLCD();
  initialiserMatrix();



  alarme.setColourA(0, 0, 255);
  alarme.setColourB(255, 0, 0);
  alarme.setVariationTiming(250);
  alarme.setTimeout(3000);
  alarme.setDistance(15);

  viseur.setAngleMin(10);
  viseur.setAngleMax(170);
  viseur.setPasParTour(2048);
  viseur.setDistanceMinSuivi(30);
  viseur.setDistanceMaxSuivi(60);
  viseur.activer();

  Serial3.begin(115200);
  WiFi.init(Serial3);
  WiFi.setPersistent();
  WiFi.endAP();
  WiFi.disconnect();

  Serial.print("Connexion au WiFi ");
  Serial.println(ssid);
  int status = WiFi.begin(ssid, pass);

  if (status == WL_CONNECTED) {
    Serial.println("WiFi connecté !");
    IPAddress ip = WiFi.localIP();
    Serial.print("Adresse IP : ");
    Serial.println(ip);
  } else {
    Serial.println("Connexion WiFi échouée.");
  }

  Client.setServer("arduino.nb.shawinigan.info", 1883);
  Client.setCallback(mqttCallback);
}

void loop() {
  _currentTime = millis();
  tacheLireDistance();
  viseur.update();
  alarme.update();
  tacheAffichageLCD();
  tacheCommande();
  tacheEffacerSymbole();


  if (!Client.connected()) reconnectMQTT();
  Client.loop();

  if (millis() - lastMqttSend1 > MQTT_SEND_INTERVAL1) {
    sendMQTTData1();
    lastMqttSend1 = millis();
  }
  if (millis() - lastMqttSend2 > MQTT_SEND_INTERVAL2) {
    sendMQTTData2();
    lastMqttSend2 = millis();
  }
  potValue = analogRead(potPin);  // 0–1023
  potPourcent = map(potValue, 0, 1023, 0, 100);
}





void reconnectMQTT() {
  if (millis() - lastMqttAttempt < 5000) return;

  lastMqttAttempt = millis();

  if (!Client.connected()) {
    Serial.print("Connexion MQTT...");
    if (Client.connect("2413335", "etdshawi", "shawi123")) {
      Serial.println("Connecté");

      Client.subscribe("etd/30/motor", 0);
      Client.subscribe("etd/30/color", 0);
      Client.publish("etd/30/motor", "{\"motor\":1}");
      Serial.println("Réabonné aux topics.");
    } else {
      Serial.print("Échec : ");
      Serial.println(Client.state());
    }
  }
}


void sendMQTTData1() {
  if (!Client.connected()) {
    Serial.println("MQTT non connecté, envoi annulé.");
    return;
  }

  static char message[200];
  char distStr[8], angleStr[8], tStr[8], hStr[8];
  int motor = (viseur.getAngle() > 10) ? 1 : 0;
  char potStr[5];
  itoa(potPourcent, potStr, 10);

  dtostrf(distance, 4, 1, distStr);
  dtostrf(viseur.getAngle(), 4, 1, angleStr);


  sprintf(message,
          "{\"number\":\"2413335\",\"name\":\"Daniel ONDO\", \"uptime\":%lu, \"dist\":%s, \"angle\":%s, \"motor\":%d, \"color\":\"#2200ff\", \"pot\":%s}",
          millis() / 1000, distStr, angleStr, motor, potStr);

  Serial.print("Message envoyé : ");
  Serial.println(message);

  Client.publish("etd/30/data", message);
}

void sendMQTTData2() {
  if (!Client.connected()) return;

  static char message[150];
  snprintf(message, sizeof(message),
           "{\"line1\":\"%s\", \"line2\":\"%s\"}", lcdLine1.c_str(), lcdLine2.c_str());

  Serial.println("🔄 Envoi message LCD JSON");
  Serial.println(message);

  Client.publish("etd/30/data", message);
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message recu [");
  Serial.print(topic);
  Serial.print("] ");

  // Convertir le payload en String
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // === GESTION MOTEUR ===
  if (strcmp(topic, "etd/30/motor") == 0) {
    int idx = message.indexOf("\"motor\":");
    if (idx != -1) {
      int val = message.substring(idx + 8).toInt();
      Serial.print("Commande moteur reçue : ");
      Serial.println(val);
      if (val == 1) {
        viseur.activer();
        motorEnabled = true;  // Active le moteur
        Serial.println("Moteur activé via MQTT");
      } else if (val == 0) {
        motorEnabled = false;  // Désactive le moteur
        viseur.desactiver();
        Serial.println("Moteur désactivé via MQTT");
      }
    } else {
      Serial.println("Champ 'motor' non trouvé");
    }
    return;
  }



  // === GESTION COULEUR ===
  if (strcmp(topic, "etd/30/color") == 0) {
    int colorIndex = message.indexOf("#");
    if (colorIndex != -1 && message.length() >= colorIndex + 7) {
      String hexColor = message.substring(colorIndex + 1, colorIndex + 7);  // exemple "FF0000"
      long number = strtol(hexColor.c_str(), NULL, 16);
      int r = (number >> 16) & 0xFF;
      int g = (number >> 8) & 0xFF;
      int b = number & 0xFF;
      alarme.setColourA(r, g, b);
      Serial.print("Nouvelle couleur : ");
      Serial.print(r);
      Serial.print(", ");
      Serial.print(g);
      Serial.print(", ");
      Serial.println(b);

    } else {
      Serial.println("Format de couleur invalide");
    }
    return;
  }

  Serial.println(" Topic non reconnu");
}