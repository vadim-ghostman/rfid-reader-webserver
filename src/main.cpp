#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "time.h"

#include <Wire.h>
#include <Adafruit_PN532.h>

#include <SPI.h>
#include <Preferences.h>
#include "SPIFFS.h"

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
// #include <ESPAsyncTCP.h>

#define PN532_SCK  (19)
#define PN532_MOSI (23)
#define PN532_SS   (22)
#define PN532_MISO (21)

#define BUZZER     (5)
#define LED_RED    (17)
#define LED_BLUE   (15)

#define MAX_TT_RECORDS 60
#define MAX_DB_RECORDS 50

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

Preferences prefs;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// Replace with your network credentials
char* ssid = "my_little_network";
const char* password = "q6t4xat2";

// HTML content for the web server
String index_html = "";
int index_to_write_in_tt = -1;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String lastName = "----";
String lastCode = "";
String lastTime = "";

int get_first_free_from_tt() {
    prefs.begin("timetable");
    for (int i = 0; i < MAX_TT_RECORDS; i++) {
        String record = prefs.getString(String(i).c_str());
        if (record.length() == 0) {
            prefs.end();
            return i;
        }
    }
    return MAX_TT_RECORDS;
}

void update_tt_index() {
    index_to_write_in_tt = get_first_free_from_tt();
}

String get_first_free_from_db() {
    prefs.begin("database");
    for (int i = 0; i < MAX_DB_RECORDS; i++) {
        String code = prefs.getString(String(i).c_str());
        if (code.length() == 0) {
            prefs.end();
            return String(i);
        }
    }
    return String(MAX_DB_RECORDS);
}

void add_to_db_by_number(String number, const char* code, String name) {
    prefs.begin("database");
    int codeLen = prefs.putString(number.c_str(), code);
    int nameLen = prefs.putString(code, name);
	Serial.printf("Added %s (%d) with code %s (%d) and number %s\n", name.c_str(), nameLen, code, codeLen, number.c_str());
    prefs.end();
}

bool is_user_in_db(String code) {
    prefs.begin("database");
    for (int i = 0; i < 50; i++) {
        String db_code = prefs.getString(String(i).c_str());
        if (db_code.length() > 0 && db_code == code) {
            prefs.end();
            return true;
        } else if (db_code.length() == 0) {
            prefs.end();
            return false;
        }
    }
    prefs.end();
    return false;
}

String get_records_from_tt() {
    update_tt_index();
    int lim = index_to_write_in_tt;
    String sendData = "TIMETABLE_DATA::";
    prefs.begin("timetable");
    for (int i = 0; i < lim; i++) {
        String record = prefs.getString(String(i).c_str());
        if (record.length() > 0)
            sendData += record + "::";
        else
            break;
    }
    prefs.end();
    sendData = sendData.substring(0, sendData.length() - 2);
    return sendData;
}

String get_records_from_db() {
    int lim = get_first_free_from_db().toInt();
    String sendData = "TABLE_DATA::";
    prefs.begin("database");
    for (int i = 0; i < lim; i++) {
        String code = prefs.getString(String(i).c_str());
        if (code.length() > 0) {
            String name = prefs.getString(code.c_str());
            sendData += name + "|" + code + "::";
        } else
            break;
    }
    prefs.end();
    sendData = sendData.substring(0, sendData.length() - 2);
    return sendData;
}

String get_time(){
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return "Failed to obtain time";
    }

    Serial.println(&timeinfo, "%d.%B.%Y %H:%M:%S");
    
    String seconds = timeinfo.tm_sec < 10 ? "0" + String(timeinfo.tm_sec) : String(timeinfo.tm_sec);
    String minutes = timeinfo.tm_min < 10 ? "0" + String(timeinfo.tm_min) : String(timeinfo.tm_min);
    String hours = timeinfo.tm_hour < 10 ? "0" + String(timeinfo.tm_hour) : String(timeinfo.tm_hour);
    String day = timeinfo.tm_mday < 10 ? "0" + String(timeinfo.tm_mday) : String(timeinfo.tm_mday);
    String month = timeinfo.tm_mon + 1 < 10 ? "0" + String(timeinfo.tm_mon + 1) : String(timeinfo.tm_mon + 1);
    String year = String(timeinfo.tm_year + 1900);

    // TODO: fix one digit time in seconds, hours and minutes
    return day + "." + month + "." + year + " " + hours + ":" + minutes + ":" + seconds;
}

bool write_time_to_tt(String name) {
    if (index_to_write_in_tt == MAX_TT_RECORDS) return false;
    String time = get_time();
    prefs.begin("timetable");
    if (index_to_write_in_tt == -1)
        index_to_write_in_tt = get_first_free_from_tt();
    else
        index_to_write_in_tt = index_to_write_in_tt % MAX_TT_RECORDS;
    int recordLen = prefs.putString(String(index_to_write_in_tt).c_str(), (name + "|" + time).c_str());
    prefs.end();
    return recordLen > 0;
}

void clear_tt() {
    prefs.begin("timetable");
    prefs.clear();
    Serial.println("Timetable cleared");
    prefs.end();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        if (strcmp((char*)data, "DB_CLEAR") == 0) {
            prefs.begin("database");
            prefs.clear();
            prefs.end();
            Serial.println("Database cleared");
            return;
        } else if (strcmp((char*)data, "TEST") == 0) {
            // delay(3000);
            // ws.textAll("REQUEST_NAME");
            prefs.begin("database");
            Serial.printf("Free entries: %u\n", prefs.freeEntries());
            prefs.end();
            return;
        } else if (strcmp((char*)data, "RELOAD_TABLE") == 0) {
            // TABLE_DATA::name1|code1::name2|code2::name3|code3
            String sendData = get_records_from_db();
            ws.textAll(sendData);
            return;
        } else if (strcmp((char*)data, "RELOAD_TIMETABLE") == 0) {
            String sendData = get_records_from_tt();
            ws.textAll(sendData);
        } else {
            lastName = (char*)data;
            String number = get_first_free_from_db();
            if (!is_user_in_db(lastCode))
                add_to_db_by_number(number, lastCode.c_str(), lastName);
        }
    }
    return;
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
            void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void initWebSocket() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

void setup() {
	Serial.begin(9600);
	Serial.println("Connecting...");

	// Connect to Wi-Fi
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
		Serial.println("Connecting to WiFi...");
	}
	Serial.println("Connected to WiFi");

    if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    String t = get_time();
    while (t == "Failed to obtain time") {
        delay(500);
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        t = get_time();
    }

	Serial.print("ESP32 IP Address: ");
	Serial.println(WiFi.localIP());

    initWebSocket();

	// Serve the HTML page
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
	});

	server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/style.css", "text/css");
	});

	server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/main.js", "text/js");
	});

	// Start server
	server.begin();

	pinMode(BUZZER, OUTPUT);
	pinMode(LED_RED, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);

	nfc.begin();

	uint32_t versiondata = nfc.getFirmwareVersion();
	if (!versiondata) {
		Serial.print("Didn't find PN53x board");
        ESP.restart();
		while (1); // halt
	}

    update_tt_index();
	Serial.println("Waiting for an ISIC card ...");
}


bool compare_strings(char* first, char* second) {
	if (strlen(first) == 0 || strlen (first) != strlen(second)) return false;
	for (int i = 0; i < strlen(first); i++) {
		if (first[i] != second[i]) return false;
	}
	return true;
}

bool is_code_in_database(const char* code) {
	prefs.begin("database");
	String name = prefs.getString(code);
	prefs.end();
	return name.length() > 0;
}

// bool is_number_in_database(String number) {
// 	prefs.begin("database");
// 	String code = prefs.getString(number.c_str());
// 	prefs.end();
// 	return code.length() > 0;
// }

String find_name(const char* code) {
	prefs.begin("database");
	String name = prefs.getString(code);
	prefs.end();
	return name;
}

String find_name_by_number(String number) {
	prefs.begin("database");
	String code = prefs.getString(number.c_str());
    String name = prefs.getString(code.c_str());
	prefs.end();
	return name;
}

void play_success() {
	analogWrite(BUZZER, 120);
	digitalWrite(LED_BLUE, HIGH);
	delay(500);
	digitalWrite(LED_BLUE, LOW);
	analogWrite(BUZZER, 0);
}

void play_error() {
	analogWrite(BUZZER, 230);
	digitalWrite(LED_RED, HIGH);
	delay(700);
	digitalWrite(LED_RED, LOW);
	analogWrite(BUZZER, 0);
}
 
void loop() {
	uint8_t success;
	uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
	uint8_t uidLength;

    ws.cleanupClients();
	success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

	if (success) {
        Serial.println("Card detected");
		String code_string = "";
		for (int i = 0; i < uidLength; i++)
			code_string += String(uid[i], HEX);
		const char* code = code_string.c_str();
		Serial.printf("ISIC: %s %s\n", code, is_code_in_database(code) ? "" : "(unknown)");
		if (is_code_in_database(code)) {
			play_success();
			String name = find_name(code);
            write_time_to_tt(name);
            update_tt_index();
			Serial.printf("Welcome, %s with code %s\n", name.c_str(), code);
            lastName = name;
            lastTime = get_time();
		} else {
			play_error();
            lastName = "REQUEST_NAME";
            lastCode = code;
            Serial.printf("Please enter your name for code %s\n", code);
		}
        nfc.readDetectedPassiveTargetID(uid, &uidLength);
        delay(1000);
	} else {
        Serial.println("error");
    }
    
    if (lastName != "REQUEST_NAME")
        ws.textAll(lastName + String(" | ") + lastTime);
    else
        ws.textAll(lastName);
	delay(100);
}