#include "EspUsbHost.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Preferences.h>
#include "ArduinoOTA.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"

const String sn = "0003";
const String ver = "1.1.0";
String wifiname, wifipassword;
String zebraIP;
int zebraPort;
WiFiClient client;
byte tries = 15;  // Попыткок подключения к точке доступа

String serverName;
HTTPClient http;

Preferences prefs;
AsyncWebServer WebServer(80);

const int packet = 5;//Пакет
unsigned long arrScans[5] = {};//Временные сегменты соответствуют пакету

int speed = 0;   //дюймы
int height = 0;  //мм
unsigned long previousMillis = 0;
float printInterval;
float loadInterval;
int checkPercent;  //Если больше то печатаем
String status = "idle";
int printedCount, needPrintCount;
long taskID;
unsigned long previousMillisTask = 0;
float taskDelayTime;
bool logScans, watchdog = false;
byte watchdogRetries = 1;
byte curWatchdogRetries = 0;

String curBarcode;
const int dbsize = 5; //Сколько ШК может быть на одной этикетке
String arrBarcodes[5];

String zpl;

String htmlProcessor(const String &var){

  if(var == "sn") return sn;
  if(var == "ver") return ver;
  if(var == "wifiname") return wifiname;
  if(var == "wifipassword") return wifipassword;
  if(var == "serverName") return serverName;
  if(var == "zebraIP") return zebraIP;
  if(var == "zebraPort") return String(zebraPort);
  if(var == "checkPercent") return String(checkPercent);
  if(var == "taskDelayTime") return String(taskDelayTime);
  if(var == "loadInterval") return String(loadInterval);
  if(var == "logScans" and logScans) return "checked";
  if(var == "watchdog" and watchdog) return "checked";
  if(var == "watchdogRetries") return String(watchdogRetries);

  return "";
}

void startWebServer(){
    WebServer.serveStatic("/bootstrap.css", LittleFS, "/bootstrap.css");
    WebServer.serveStatic("/bootstrap.js", LittleFS, "/bootstrap.js");
    WebServer.serveStatic("/main.css", LittleFS, "/main.css");
    WebServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->redirect("/index.html");
    });
    WebServer.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/index.html", "text/html", false, htmlProcessor);
    });
    WebServer.on("/index.html", HTTP_POST, [](AsyncWebServerRequest *request){
      savePrefs(request);
      request->send(LittleFS, "/index.html", "text/html", false, htmlProcessor);
    });
    WebServer.begin();
    Serial.println("Веб-сервер запущен");
}

void startAP(){
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ZPrint" + sn);
  startWebServer();
  Serial.println("Точка доступа включена");
  Serial.println("IP адрес: 192.168.4.1");
}

void startOTA(){
  ArduinoOTA.begin();
  Serial.println("OTA включен");
}

void setDefaultPrefs(){

  prefs.begin("prefs", false);
  
  if(!prefs.isKey("init")){

    //Системные
    prefs.putBool("logScans", false);
    prefs.putBool("watchdog", false);
    prefs.putInt("watchdogRetries", 1);
    
    //WiFi
    prefs.putString("wifiname", "PlasticRepublic");
    prefs.putString("wifipassword", "horizon772fec");

    //Сеть
    prefs.putString("serverName", "https://seven.9330077.ru/1c/ZebraPrint");

    //Принтер
    prefs.putString("zebraIP", "192.168.17.119");
    prefs.putInt("zebraPort", 9100);

    //Печать
    prefs.putInt("checkPercent", 50);
    prefs.putFloat("taskDelayTime", 5000);
    prefs.putFloat("loadInterval", 1700);

    prefs.putBool("init", true);
    prefs.end();

  }
}

void getPrefs(){

  prefs.begin("prefs", true);

  wifiname = prefs.getString("wifiname");
  wifipassword = prefs.getString("wifipassword");
  serverName = prefs.getString("serverName");
  zebraIP = prefs.getString("zebraIP");
  zebraPort = prefs.getInt("zebraPort");
  checkPercent = prefs.getInt("checkPercent");
  taskDelayTime = prefs.getFloat("taskDelayTime");
  loadInterval = prefs.getFloat("loadInterval");
  logScans = prefs.getBool("logScans");
  watchdog = prefs.getBool("watchdog");
  watchdogRetries = prefs.getInt("watchdogRetries");

  prefs.end();
}

void savePrefs(AsyncWebServerRequest *request){

  prefs.begin("prefs", false);

  if(request->getParam("wifiname", true)->value()!=wifiname) {
    wifiname = request->getParam("wifiname", true)->value();
    prefs.putString("wifiname", wifiname);
  }
  if(request->getParam("wifipassword", true)->value()!=wifipassword) {
    wifipassword = request->getParam("wifipassword", true)->value();
    prefs.putString("wifipassword", wifipassword);
  }
  if(request->getParam("serverName", true)->value()!=serverName) {
    serverName = request->getParam("serverName", true)->value();
    prefs.putString("serverName", serverName);
  }
  if(request->getParam("zebraIP", true)->value()!=zebraIP) {
    zebraIP = request->getParam("zebraIP", true)->value();
    prefs.putString("zebraIP", zebraIP);
  }
  if(request->getParam("zebraPort", true)->value()!=String(zebraPort)) {
    zebraPort = (request->getParam("zebraPort", true)->value()).toInt();
    prefs.putInt("zebraPort", zebraPort);
  }
  if(request->getParam("checkPercent", true)->value()!=String(checkPercent)) {
    checkPercent = (request->getParam("checkPercent", true)->value()).toInt();
    prefs.putInt("checkPercent", checkPercent);
  }
  if(request->getParam("taskDelayTime", true)->value()!=String(taskDelayTime)) {
    taskDelayTime = (request->getParam("taskDelayTime", true)->value()).toFloat();
    prefs.putFloat("taskDelayTime", taskDelayTime);
  }
  if(request->getParam("loadInterval", true)->value()!=String(loadInterval)) {
    loadInterval = (request->getParam("loadInterval", true)->value()).toFloat();
    prefs.putFloat("loadInterval", loadInterval);
  }

  if(request->hasParam("logScans", true) and !logScans){
    logScans = true;
    prefs.putBool("logScans", true);
  }
  if(!request->hasParam("logScans", true) and logScans){
    logScans = false;
    prefs.putBool("logScans", false);
  }

  if(request->hasParam("watchdog", true) and !watchdog){
    watchdog = true;
    prefs.putBool("watchdog", true);
  }
  if(!request->hasParam("watchdog", true) and watchdog){
    watchdog = false;
    prefs.putBool("watchdog", false);
  }

  if(request->getParam("watchdogRetries", true)->value()!=String(watchdogRetries)) {
    watchdogRetries = (request->getParam("watchdogRetries", true)->value()).toInt();
    prefs.putInt("watchdogRetries", watchdogRetries);
  }
  
  prefs.end();
  delay(500);
}

void getPrintTask() {

  //Сделаем задержку
  if (millis() - previousMillisTask >= taskDelayTime) {
    previousMillisTask = millis();

    if (WiFi.status() == WL_CONNECTED) {
      String serverPath = serverName + "/getTask.php?sn="+sn;
      http.begin(serverPath.c_str());
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {

        String payload = http.getString();
        StaticJsonDocument<100000> doc;
        deserializeJson(doc, payload);

        String json_zpl = doc["zpl"];
        long json_id = doc["id"];
        String json_barcodes0 = doc["barcodes"][0];
        String json_barcodes1 = doc["barcodes"][1];
        String json_barcodes2 = doc["barcodes"][2];
        String json_barcodes3 = doc["barcodes"][3];
        String json_barcodes4 = doc["barcodes"][4];
        int json_height = doc["height"];
        int json_speed = doc["speed"];
        int json_count = doc["count"];
        byte json_curWatchdogRetries = doc["wd_retries"];

        if (json_id == 0) {
          Serial.println("Заданий нет");
          return;
        }

        zpl = json_zpl;
        zpl.replace("<<экз>>", String(packet));

        arrBarcodes[0] = json_barcodes0;
        arrBarcodes[1] = json_barcodes1;
        arrBarcodes[2] = json_barcodes2;
        arrBarcodes[3] = json_barcodes3;
        arrBarcodes[4] = json_barcodes4;

        taskID = json_id;
        height = json_height;
        speed = json_speed;
        printInterval = (height * packet) / (speed * 25.4) * 1000;
        needPrintCount = json_count;
        curWatchdogRetries = json_curWatchdogRetries;

        status = "ready";

      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
  }
}

void resetStats() {
  //Очистка статистики
  Serial.println("Очистка статистики");
  for (int i = 0; i < packet; i++) {
    arrScans[i] = 0;
  }
}

void refreshPrintCount() {
  if (WiFi.status() == WL_CONNECTED) {
    String serverPath = serverName + "/refreshPrintedCount.php?id=" + taskID + "&printedcount=" + packet;
    http.begin(serverPath.c_str());
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.println("Обновили напечатаное кол-во +" + String(packet));
      printedCount += packet;
    }
  }
}

void refreshWatchdogRetries() {
  if (WiFi.status() == WL_CONNECTED) {
    String serverPath = serverName + "/refreshWatchdogRetries.php?id=" + taskID;
    http.begin(serverPath.c_str());
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.println("Обновили попытки ватчдога");
    }
  }
}

String rus(String source) {
  source.replace("^XA", "^XA^CI28");
  source.replace("^PP", "");
  return source;
}

bool arrFind(String needle) {
  for (int i = 0; i < dbsize; i++) {
    if (arrBarcodes[i] == needle) return true;
  }
  return false;
}

void saveScan(int period) {
  if(period<0) return;
  if (!arrScans[period] and arrFind(curBarcode)) {
    Serial.println("Записываем в период " + String(period + 1));
    if (!arrScans[period]) arrScans[period] = millis();
  }
}

int getScanStats() {
  float passedPeriods = 0;
  for (int i = 0; i < packet; i++) {
    if (arrScans[i]) passedPeriods++;
  }
  return passedPeriods / packet * 100;
}

void print() {
  status = "printing";
  refreshPrintCount();
  Serial.println("Напечатано " + String(printedCount) + "/" + String(needPrintCount));
  zpl = rus(zpl);
  client.connect(zebraIP.c_str(), zebraPort);
  client.print(zpl);
  client.stop();
  previousMillis = millis();
}

void printNotPermit() {
  Serial.println("Проверка не пройдена. Печать остановлена.");
  client.connect(zebraIP.c_str(), zebraPort);
  if(!client.connected()) Serial.println("Ошибка! Соединение не установлено.");
  String warning = "^XA^PR3^MD10^LH1,1^FO30,100^FB550,5,0,L^A@N,40,40,E:TT0003M_.TTF^FDКонтроль не пройден. Печать остановлена. Напечатано " + String(printedCount) + "/" + String(needPrintCount) + "\\&Осталось " + String(needPrintCount-printedCount) + "^PQ1^XZ";
  client.print(rus(warning));
}

void printWatchdog(){
  Serial.println("Сработал ватчдог. Перезагрузка для повторной печати");
  client.connect(zebraIP.c_str(), zebraPort);
  if(!client.connected()) Serial.println("Ошибка! Соединение не установлено.");
  String warning = "^XA^PR3^MD10^LH1,1^FO30,100^FB550,5,0,L^A@N,40,40,E:TT0003M_.TTF^FDСработал ватчдог. Перезагрузка для повторной печати ^PQ1^XZ";
  client.print(rus(warning));
}

void taskEnd() {
  client.connect(zebraIP.c_str(), zebraPort);
  if(!client.connected()) Serial.println("Ошибка! Соединение не установлено.");
  String end = "^XA^PR3^MD10^LH1,1^FO30,100^FB550,3,0,L^A@N,40,40,E:TT0003M_.TTF^FDЗавершено задание " + String(taskID) + ". Напечатано " + String(printedCount) + " этикеток^PQ1^XZ";
  client.print(rus(end));
  client.stop();
}

void printPause(){
  client.connect(zebraIP.c_str(), zebraPort);
  if(!client.connected()) Serial.println("Ошибка! Соединение не установлено.");
  String pause = "^XA^PP^XZ";
  client.print(pause);
  client.stop();
}

void scan() {

  unsigned long currentMillis = millis() - loadInterval;
  long time = currentMillis - previousMillis;

  //Записываем сканы в периоды
  int period = time / (printInterval / packet);
  if (period < packet) saveScan(period);

  curBarcode = "";
}

void continuePrint() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= loadInterval + printInterval) {
    previousMillis = currentMillis;
    //arrScans[dbsize-2] > 0 and  - 151025 Отключено по устному заданию Теренко
    if (getScanStats() >= checkPercent) {
      Serial.println("По статистике было " + String(getScanStats()) + "% сканирующихся этикеток");
      resetStats();
      print();
    } else {
      if(!watchdog or (watchdog and curWatchdogRetries>=watchdogRetries)){
        printNotPermit();
        closeTask();
      } else if(watchdog and curWatchdogRetries<watchdogRetries){
        refreshWatchdogRetries();
        printWatchdog();
        esp_restart();
        //curWatchdogRetries
      }
    }
  }
}

class MyEspUsbHost : public EspUsbHost {
  void onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier) {
    if (' ' <= ascii && ascii <= '~') {
      curBarcode += char(ascii);
    } else if (ascii == '\r') {
      if(logScans) Serial.println("Отсканировано: " + curBarcode);
      scan();
    }
  };
};

MyEspUsbHost usbHost;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Серийный номер " + sn);

  setDefaultPrefs();
  getPrefs();
  LittleFS.begin(true);

  WiFi.setHostname(("ZPrint"+sn).c_str());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  WiFi.begin(wifiname, wifipassword);
  while (--tries && WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Не получилось подключиться к WiFi");
    Serial.println("Имя сети: " + wifiname);
    Serial.println("Пароль сети: " + wifipassword);
    startAP();
    startOTA();
  } else {
    Serial.println("");
    Serial.println("WiFi подключен");
    Serial.print("IP адрес: ");
    Serial.println(WiFi.localIP());
    startOTA();
    startWebServer();
  }

  usbHost.begin();
  usbHost.setHIDLocal(HID_LOCAL_Russia);
}

void closeTask() {
  status = "idle";
  if (WiFi.status() == WL_CONNECTED) {
    String serverPath = serverName + "/closeTask.php?id=" + taskID;
    http.begin(serverPath.c_str());
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.println("Закрыли задание №" + String(taskID));
      printedCount = 0;
      taskID = 0;
    }
  }
}

void loop() {
  usbHost.task();
  if (status == "printing" and printedCount >= needPrintCount) {
    taskEnd();
    printPause(); //Добавлено Теренко 201025 №375114
    closeTask();
  }

  if (status == "idle") getPrintTask();

  if (status == "printing") {
    continuePrint();
  }

  if (status == "ready") {
    resetStats();
    Serial.println("Начинаем печатать задание №" + String(taskID));
    print();
  }
}
