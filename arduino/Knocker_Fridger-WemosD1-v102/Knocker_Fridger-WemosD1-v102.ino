#include <DHT.h>
#include <ESP8266WiFi.h>

#define ledKYL  D2
#define ledHEAT D3
#define DHTPIN D5
#define DHTTYPE DHT22
#define outKYL D7
#define outHEAT D6
#define USER_AGENT "UbidotsESP8266"
#define VERSION "1.6"
#define HTTPSERVER "things.ubidots.com"
#define SERVER "translate.ubidots.com"
#define PORT 9012
#define SETID ""            // Ubidots variable ID, Setpoint
#define TEMID ""            // Ubidots variable ID, Temperature
#define HUMID ""            // Ubidots variable ID, Humidituý
#define HEAID ""            // Ubidots variable ID, On/Off time heat
#define COLID ""            // Ubidots variable ID, On/Off time cold
#define TOKEN ""            // Ubidots TOKEN
#define WIFISSID ""         // Your WiFi SSID
#define PASSWORD ""         // Your WiFi password

unsigned long previousMillisCompressor = 0;         // will store last time the compressor ran
unsigned long prevMillisUbidots  = 0;               // will store last time Ubidots was updated
unsigned long prevMillisSetpoint = 0;               // will store last time Setpoint was updated
unsigned long prevMillisDHT      = 0;               // will store last time the DHT sensor was updated
const long interUbidots  = 60000;                   // interval at which to update UbiDots (milliseconds)
const long interSetpoint = 30000;                   // interval at which to get Setpoint from UbiDots (milliseconds)
const long interDHT      = 2000;                    // interval at which to update DHT sensor (milliseconds)
long OnTime  = 240000;                              // milliseconds on-time of the compressor
long OffTime = 600000;                              // milliseconds off-time of the compressor

float h = 0, t = 0, value = 0;
int timeHeat = 0, timeCold = 0;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;

bool inRange(float val, float minimum, float maximum) {
  return ((minimum <= val) && (val <= maximum));
}

void updateDHT() {
  unsigned long curMillisDHT = millis();
  if (curMillisDHT - prevMillisDHT >= interDHT) {
    prevMillisDHT = curMillisDHT;

    h = dht.readHumidity();
    t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("Error reading the sensor");
      return;
    }
    Serial.println("");
    Serial.println("----------------------- UPDATING DHT SENSOR ----------------------- ");
    Serial.print("Temperature: "); Serial.print(t); Serial.print(" *C, "); Serial.print("Humidity: "); Serial.print(h); Serial.println(" %");
    Serial.println("");
    Serial.print("Setpoint:    "); Serial.print(value); Serial.println(" *C");
  }
}

void updateUbidots() {
  unsigned long curMillisUbidots = millis();
  if (curMillisUbidots - prevMillisUbidots >= interUbidots) {
    prevMillisUbidots = curMillisUbidots;   // save the last time Ubidots was updated

    uint16_t i;
    String all = "[";
    all += "{\"variable\": \"";
    all += TEMID;
    all += "\", \"value\":";
    all += t;
    all += "}";

    all += ",{\"variable\": \"";
    all += HUMID;
    all += "\", \"value\":";
    all += h;
    all += "}";

    all += ",{\"variable\": \"";
    all += HEAID;
    all += "\", \"value\":";
    all += timeHeat;
    all += "}";

    all += ",{\"variable\": \"";
    all += COLID;
    all += "\", \"value\":";
    all += timeCold;
    all += "}";
    all += "]";
    i = all.length();

    String toPost = "POST /api/v1.6/collections/values/?force=true HTTP/1.1\r\n"
                    "Host: things.ubidots.com\r\n"
                    "User-Agent:" + String(USER_AGENT) + "/" + String(VERSION) + "\r\n"
                    "X-Auth-Token: " + String(TOKEN) + "\r\n"
                    "Connection: close\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: " + String(i) + "\r\n"
                    "\r\n"
                    + all +
                    "\r\n";

    if (client.connect(HTTPSERVER, 80)) {
      Serial.println("");
      Serial.println("----------------------- SENDING TO UBIDOTS ----------------------- ");
      Serial.print("Temperature: "); Serial.print(t); Serial.print("*C  "); Serial.print("Humidity: "); Serial.print(h); Serial.println("%");
      Serial.print("On/Off Heat: "); Serial.print(timeHeat); Serial.print("On/off Cold: "); Serial.print(timeCold);
      client.print(toPost);
    }

    int timeout = 0;
    while (!client.available() && timeout < 5000) {
      timeout++;
      delay(1);
    }
    client.stop();
    return;
  }
}

void getSetpointUbidots() {
  unsigned long curMillisSetpoint = millis();
  if (curMillisSetpoint - prevMillisSetpoint >= interSetpoint) {
    prevMillisSetpoint = curMillisSetpoint;   // save the last time Setpoint was retrived from Ubidots

    char* response = (char *) malloc(sizeof(char) * 40);
    char* data = (char *) malloc(sizeof(char) * 700);
    sprintf(data, "%s/%s|GET|%s|%s", USER_AGENT, VERSION, TOKEN, SETID);
    sprintf(data, "%s|end", data);
    //Serial.println(data);

    if (client.connect(SERVER, PORT)) {
      Serial.println("");
      Serial.println("----------------------- GETTING SETPOINT FROM UBIDOTS ----------------------- ");
      client.print(data);
    }

    int timeout = 0;
    free(data);

    while (!client.available() && timeout < 30000) {
      timeout++;
      if (timeout >= 29999) {
        Serial.println("Timeout, waited for 30 seconds!");
        return;
      }
      delay(1);
    }

    int i = 0;
    for (int i = 0; i <= 40; i++) {
      response[i] = '\0';
    }

    while (client.available()) {
      response[i++] = (char)client.read();
    }

    // Parses the answer, Expected "OK|{value}"
    char * pch = strchr(response, '|');
    if (pch != NULL) {
      float num;
      pch[0] = '0';
      num = atof(pch);
      value = num;
      free(response);
      client.stop();
      return;
    }
    free(response);
    return;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFISSID, PASSWORD);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFISSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  dht.begin();

  pinMode(outKYL, OUTPUT); digitalWrite(outKYL, HIGH);
  pinMode(ledKYL, OUTPUT); digitalWrite(ledKYL, LOW);
  pinMode(outHEAT, OUTPUT); digitalWrite(outHEAT, HIGH);
  pinMode(ledHEAT, OUTPUT); digitalWrite(ledHEAT, LOW);
}

void loop() {
  updateDHT();
  getSetpointUbidots();
  updateUbidots();

  if (t > value) {
    unsigned long currentMillisCompressor = millis();

    if (digitalRead(outHEAT) == 0) {
      digitalWrite(outHEAT, HIGH);
      digitalWrite(ledHEAT, LOW);
      timeHeat = 0;
    }

    if (inRange(t, (value - 0.5), (value + 0.5))) {
      if ((digitalRead(outKYL) == 0) && (currentMillisCompressor - previousMillisCompressor >= OnTime)) {
        digitalWrite(outKYL, HIGH);
        digitalWrite(ledKYL, LOW);
        timeCold = 0;
        previousMillisCompressor = currentMillisCompressor;
      }
    } else if ((digitalRead(outKYL) == 1) && (currentMillisCompressor - previousMillisCompressor >= OffTime)) {
      digitalWrite(outKYL, LOW);
      digitalWrite(ledKYL, HIGH);
      timeCold = 1;
      previousMillisCompressor = currentMillisCompressor;
    }
  }

  if (t < value) {
    unsigned long currentMillisCompressor = millis();

    if ((digitalRead(outKYL) == 0) && (currentMillisCompressor - previousMillisCompressor >= OnTime)) {
      digitalWrite(outKYL, HIGH);
      digitalWrite(ledKYL, LOW);
      timeCold = 0;
      previousMillisCompressor = currentMillisCompressor;
    }

    if (inRange(t, (value - 0.5), (value + 0.5))) {
      return;
    } else if (digitalRead(outHEAT) == 1) {
      digitalWrite(outHEAT, LOW);
      digitalWrite(ledHEAT, HIGH);
      timeHeat = 1;
    }
  }
}
