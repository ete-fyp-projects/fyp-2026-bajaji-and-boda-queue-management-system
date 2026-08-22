#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define EEPROM_I2C_ADDR 0x50
#define TOTAL_EEPROM_BYTES 32768

const String ADMIN_NUMBER = "+255685887046";
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define SS_ENTRY 53
#define RST_ENTRY 5
#define SS_EXIT 48
#define RST_EXIT 49
MFRC522 rfidEntry(SS_ENTRY, RST_ENTRY);
MFRC522 rfidExit(SS_EXIT, RST_EXIT);
struct RiderProfile {
	bool isActive; 
	byte rfidUID[4]; 
	char phone[16]; 
	char name[24]; 
	char plateNumber[12]; 
	char reserved[7]; 
};

const int RIDER_SIZE = sizeof(RiderProfile);
const int MAX_DATABASE_RIDERS = TOTAL_EEPROM_BYTES / RIDER_SIZE; 
String queueNames[10];
String queuePlates[10];
String queuePhones[10]; 
byte queueUIDs[10][4];
int front = 0;
int rear = 0;
bool isWaitingForScan = false;
bool registrationComplete = false; 
unsigned long scanTimeoutTimer = 0;
int slotForPendingRider = -1; 26
RiderProfile pendingRiderBuffer;
String gsmBuffer = "";
bool waitingForSMSBody = false; 
String pendingSMSSender = ""; 
void checkAndInjectDefaults();
void showQueue();
void processIncomingGSMStream(String line);
void handleEntryScan(byte *uid);
void handleExitScan(byte *uid);
void eeprom_write_block(unsigned int eeAddress, const byte* source, char length);
void eeprom_read_block(unsigned int eeAddress, byte* destination, char length);
int findDatabaseRiderByUID(byte *uid);
int findDatabaseRiderByName(String searchStr);
int findFreeSlot();
int findQueueRider(byte *uid);
void sendSMS(String targetNum, String text);
void executeAdminCommand(String msg);

void setup() {
	Serial.begin(9600); 
	Wire.begin();
	pinMode(SS_ENTRY, OUTPUT); digitalWrite(SS_ENTRY, HIGH);
	pinMode(SS_EXIT, OUTPUT); digitalWrite(SS_EXIT, HIGH);
	SPI.begin();
	rfidEntry.PCD_Init();
	delay(50);
	rfidExit.PCD_Init();
	delay(50);
	Serial.print("Entry RFID firmware: ");
	rfidEntry.PCD_DumpVersionToSerial();
	Serial.print("Exit RFID firmware: ");
	rfidExit.PCD_DumpVersionToSerial();
	lcd.init();
	lcd.backlight(); 27
	checkAndInjectDefaults();
	Serial1.begin(9600);
	delay(3000); 
	Serial1.println("AT"); delay(500);
	Serial1.println("ATE0"); delay(300); 
	Serial1.println("AT+CMGF=1"); delay(300); 
	Serial1.println("AT+CSCS=\"GSM\""); delay(300); 
	Serial1.println("AT+CSMP=17,167,0,0"); delay(300); 
	Serial1.println("AT+CNMI=2,2,0,0,0"); delay(300); 
	lcd.clear();
	lcd.setCursor(0, 0); lcd.print("Bajaji Queue");
	lcd.setCursor(0, 1); lcd.print("System Ready");
	delay(2000);
	showQueue();
}

void loop() {
	while (Serial1.available() > 0) {
		char c = Serial1.read();
		gsmBuffer += c;
		if (c == '\n') {
		processIncomingGSMStream(gsmBuffer);
		gsmBuffer = "";
		}
	}
	if (isWaitingForScan && !registrationComplete && (millis() - scanTimeoutTimer > 
	120000)) {
		isWaitingForScan = false;
		sendSMS(ADMIN_NUMBER, "Error: Registration window expired without card 
		scan.");
	}
	digitalWrite(SS_EXIT, HIGH);
	digitalWrite(SS_ENTRY, LOW);
	if (rfidEntry.PICC_IsNewCardPresent() && rfidEntry.PICC_ReadCardSerial()) {
		handleEntryScan(rfidEntry.uid.uidByte);
		rfidEntry.PICC_HaltA();
		rfidEntry.PCD_StopCrypto1();
	} 
	digitalWrite(SS_ENTRY, HIGH);
	delay(20); 
	digitalWrite(SS_ENTRY, HIGH);
	digitalWrite(SS_EXIT, LOW);
	if (rfidExit.PICC_IsNewCardPresent() && rfidExit.PICC_ReadCardSerial()) {
		handleExitScan(rfidExit.uid.uidByte);
		rfidExit.PICC_HaltA();
		rfidExit.PCD_StopCrypto1();
	}
	digitalWrite(SS_EXIT, HIGH);
}
void eeprom_write_block(unsigned int eeAddress, const byte* source, char length) 
{
	// Write first 32 bytes
	Wire.beginTransmission(EEPROM_I2C_ADDR);
	Wire.write((int)(eeAddress >> 8));
	Wire.write((int)(eeAddress & 0xFF));
	for (int i = 0; i < 32; i++) Wire.write(source[i]);
	Wire.endTransmission();
	delay(10);
	unsigned int addr2 = eeAddress + 32;
	Wire.beginTransmission(EEPROM_I2C_ADDR);
	Wire.write((int)(addr2 >> 8));
	Wire.write((int)(addr2 & 0xFF));
	for (int i = 32; i < length; i++) Wire.write(source[i]);
	Wire.endTransmission();
	delay(10);
}
void eeprom_read_block(unsigned int eeAddress, byte* destination, char length) {
	for (int chunk = 0; chunk < 2; chunk++) {
		unsigned int addr = eeAddress + (chunk * 32);
		Wire.beginTransmission(EEPROM_I2C_ADDR);
		Wire.write((int)(addr >> 8));
		Wire.write((int)(addr & 0xFF));
		Wire.endTransmission();
		Wire.requestFrom(EEPROM_I2C_ADDR, 32);
		for (int i = 0; i < 32; i++) {
			if (Wire.available()) destination[(chunk * 32) + i] = Wire.read();
		} 
	}
}
int findDatabaseRiderByUID(byte *uid) {
	for (int i = 0; i < MAX_DATABASE_RIDERS; i++) {
		RiderProfile temp;
		eeprom_read_block(i * RIDER_SIZE, (byte*)&temp, RIDER_SIZE);
		if (temp.isActive && temp.rfidUID[0] != 0xFF) {
			bool match = true;
			for (byte j = 0; j < 4; j++) {
			if (temp.rfidUID[j] != uid[j]) { match = false; break; }
		}
		if (match) return i;
		}
	}
	return -1;
}
int findDatabaseRiderByName(String searchStr) {
	searchStr.trim();
	searchStr.toLowerCase();
	char searchBuf[24];
	searchStr.toCharArray(searchBuf, 24);
	for (int i = 0; i < MAX_DATABASE_RIDERS; i++) {
		RiderProfile temp;
		eeprom_read_block(i * RIDER_SIZE, (byte*)&temp, RIDER_SIZE);
		if (temp.isActive && (byte)temp.name[0] != 0xFF && temp.name[0] != 0) {
		char lower[24];
		memset(lower, 0, 24);
		for (int j = 0; j < 24 && temp.name[j] != '\0'; j++) {
			char ch = temp.name[j];
		if (ch >= 'A' && ch <= 'Z') ch += 32;
			lower[j] = ch;
		}
		if (strstr(lower, searchBuf) != NULL) return i;
		}
	}
	return -1;
}
int findFreeSlot() {
for (int i = 0; i < MAX_DATABASE_RIDERS; i++) {
RiderProfile temp;
eeprom_read_block(i * RIDER_SIZE, (byte*)&temp, RIDER_SIZE); 30
if (!temp.isActive) return i;
}
return -1;
}
int findQueueRider(byte *uid) {
for (int i = front; i < rear; i++) {
bool same = true;
for (int j = 0; j < 4; j++) {
if (queueUIDs[i][j] != uid[j]) { same = false; break; }
}
if (same) return i;
}
return -1;
}
void showQueue() {
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("Queue: ");
lcd.print(rear - front);
lcd.setCursor(0, 1);
if (front < rear) {
lcd.print(queueNames[front]);
lcd.print(" ");
lcd.print(queuePlates[front]);
} else {
lcd.print("No Riders");
}
}
void sendSMS(String targetNum, String text) {
Serial.print("[SMS] Sending to: "); Serial.println(targetNum);
Serial.print("[SMS] Text: "); Serial.println(text);
delay(200);
while (Serial1.available()) {
char flushed = Serial1.read();
Serial.print(flushed); 
} 31
Serial.println();
Serial1.print("AT+CMGS=\"");
Serial1.print(targetNum);
Serial1.println("\"");
unsigned long start = millis();
bool gotPrompt = false;
while (millis() - start < 10000) {
if (Serial1.available()) {
char c = Serial1.read();
Serial.write(c); 
if (c == '>') {
gotPrompt = true;
break;
}
}
}
if (!gotPrompt) {
Serial.println("\n[SMS] ERROR: No '>' prompt. SMS not sent.");
return;
}
delay(100); 
Serial1.print(text);
delay(100);
Serial1.write(26); 
Serial.println("[SMS] Waiting for +CMGS confirmation...");
unsigned long sent = millis();
while (millis() - sent < 8000) {
while (Serial1.available()) {
char c = Serial1.read();
Serial.write(c);
gsmBuffer += c;
}
if (gsmBuffer.indexOf("OK") != -1 || gsmBuffer.indexOf("+CMGS") != -1) break;
}
gsmBuffer = ""; 
while (Serial1.available()) Serial1.read(); 32
Serial.println("\n[SMS] Done.");
}
void processIncomingGSMStream(String line) {
line.trim();
if (line.length() == 0) return;
Serial.print("[GSM] "); Serial.println(line); 
if (line.startsWith("+CMT:")) {
if (line.indexOf(ADMIN_NUMBER) != -1) {
waitingForSMSBody = true; 
} else {
waitingForSMSBody = false; 
}
return;
}
if (waitingForSMSBody) {
waitingForSMSBody = false; 
executeAdminCommand(line); 
}
}
void executeAdminCommand(String msg) {
Serial.print("[CMD] "); Serial.println(msg);
if (msg.startsWith("ADD#")) {
int h1 = msg.indexOf('#');
int h2 = msg.indexOf('#', h1 + 1);
int h3 = msg.indexOf('#', h2 + 1);
if (h2 == -1 || h3 == -1) {
sendSMS(ADMIN_NUMBER, "Format error. Use: ADD#Phone#Name#Plate");
return;
}
memset(&pendingRiderBuffer, 0, sizeof(RiderProfile));
pendingRiderBuffer.isActive = true;
msg.substring(h1 + 1, h2).toCharArray(pendingRiderBuffer.phone, 16); 33
msg.substring(h2 + 1, h3).toCharArray(pendingRiderBuffer.name, 24);
msg.substring(h3 + 1).toCharArray(pendingRiderBuffer.plateNumber, 12);
slotForPendingRider = findFreeSlot();
if (slotForPendingRider == -1) {
sendSMS(ADMIN_NUMBER, "Error: Database full (512 riders max).");
return;
}
registrationComplete = false; 
isWaitingForScan = true;
sendSMS(ADMIN_NUMBER, "Information received. Please scan the card now on the 
ENTRY reader...");
scanTimeoutTimer = millis();
}
else if (msg.startsWith("REMOVE#")) {
String targetName = msg.substring(7);
int slot = findDatabaseRiderByName(targetName);
if (slot == -1) {
sendSMS(ADMIN_NUMBER, "Driver not found in the system.");
return;
}
RiderProfile temp;
eeprom_read_block(slot * RIDER_SIZE, (byte*)&temp, RIDER_SIZE);
temp.isActive = false;
eeprom_write_block(slot * RIDER_SIZE, (const byte*)&temp, RIDER_SIZE);
sendSMS(ADMIN_NUMBER, "Update complete. " + String(temp.name) + " has been 
removed.");
}
else if (msg.startsWith("FIND#")) {
String searchName = msg.substring(5);
int slot = findDatabaseRiderByName(searchName);
if (slot == -1) {
sendSMS(ADMIN_NUMBER, "Driver not found.");
return;
}
RiderProfile r;
eeprom_read_block(slot * RIDER_SIZE, (byte*)&r, RIDER_SIZE);
sendSMS(ADMIN_NUMBER, 34
"Rider: " + String(r.name) +
"\nPhone: " + String(r.phone) +
"\nPlate: " + String(r.plateNumber));
}
else if (msg == "STATUS") {
int activeCount = 0;
for (int i = 0; i < MAX_DATABASE_RIDERS; i++) {
RiderProfile temp;
eeprom_read_block(i * RIDER_SIZE, (byte*)&temp, RIDER_SIZE);
if (temp.isActive && (byte)temp.name[0] != 0xFF) activeCount++;
}
sendSMS(ADMIN_NUMBER,
"Total Drivers: " + String(activeCount) + "/512\nIn Queue: " + String(rear 
- front));
}
else if (msg == "LIST") {
String outMsg = "Registered Drivers:\n";
int count = 0;
for (int i = 0; i < MAX_DATABASE_RIDERS; i++) {
RiderProfile r;
eeprom_read_block(i * RIDER_SIZE, (byte*)&r, RIDER_SIZE);
if (r.isActive && (byte)r.name[0] != 0xFF && r.name[0] != 0) {
count++;
outMsg += String(count) + "." + String(r.name) + " [" + 
String(r.plateNumber) + "]\n";
if (outMsg.length() > 140) { outMsg += "...more exist."; break; }
}
}
if (count == 0) outMsg = "No drivers registered.";
sendSMS(ADMIN_NUMBER, outMsg);
}
else {
sendSMS(ADMIN_NUMBER, "Unknown command. Valid: ADD#, REMOVE#, FIND#, STATUS, 
LIST");
}
} 35
void handleEntryScan(byte *uid) {
if (isWaitingForScan) {
isWaitingForScan = false;
registrationComplete = true; 
isWaitingForScan = false;
scanTimeoutTimer = 0;
memcpy(pendingRiderBuffer.rfidUID, uid, 4);
e
if (slotForPendingRider == -1) {
sendSMS(ADMIN_NUMBER, "Error: No slot available.");
return;
}
eeprom_write_block(slotForPendingRider * RIDER_SIZE, (const
byte*)&pendingRiderBuffer, RIDER_SIZE);
slotForPendingRider = -1; 
sendSMS(ADMIN_NUMBER,
"Registered: " + String(pendingRiderBuffer.name) +
" | Plate: " + String(pendingRiderBuffer.plateNumber));
delay(1000); 
String riderPhone = String(pendingRiderBuffer.phone);
riderPhone.trim();
Serial.print("[REG] Rider phone to welcome: '");
Serial.print(riderPhone); Serial.println("'");
if (riderPhone.length() > 6) {
sendSMS(riderPhone,
"Karibu kwenye BQM system, ndugu " +
String(pendingRiderBuffer.name) + "! " +
"Namba yako ya usajili imefanikiwa. Skani kadi yako kuingia kwenye 
foleni.");
} else {
Serial.println("[REG] WARNING: Rider phone is empty or too short — welcome 
SMS skipped.");
}
showQueue();
return;
} 36
int slot = findDatabaseRiderByUID(uid);
if (slot == -1) {
lcd.clear(); lcd.setCursor(0,0); lcd.print("Unknown Card");
delay(2000); showQueue(); return;
}
if (rear >= 10) {
lcd.clear(); lcd.setCursor(0,0); lcd.print("Queue Full!");
delay(2000); showQueue(); return;
}
if (findQueueRider(uid) != -1) {
lcd.clear(); lcd.setCursor(0,0); lcd.print("Already Queued");
delay(2000); showQueue(); return;
}
RiderProfile r;
eeprom_read_block(slot * RIDER_SIZE, (byte*)&r, RIDER_SIZE);
queueNames[rear] = String(r.name);
queuePlates[rear] = String(r.plateNumber);
queuePhones[rear] = String(r.phone); 
memcpy(queueUIDs[rear], uid, 4);
rear++;
showQueue();
}
void handleExitScan(byte *uid) {
int pos = findQueueRider(uid);
if (pos == -1) {
lcd.clear();
lcd.setCursor(0, 0); lcd.print("Hujaingia");
lcd.setCursor(0, 1); lcd.print("kwenye foleni");
delay(2000); showQueue(); return;
}
if (pos != front) {
String leavingName = queueNames[pos];
for (int i = pos; i < rear - 1; i++) {
queueNames[i] = queueNames[i + 1];
queuePlates[i] = queuePlates[i + 1];
queuePhones[i] = queuePhones[i + 1];
memcpy(queueUIDs[i], queueUIDs[i + 1], 4);
} 37
rear--;
lcd.clear();
lcd.setCursor(0, 0); lcd.print(leavingName.substring(0, 16));
lcd.setCursor(0, 1); lcd.print("Ameondoka");
delay(2000);
showQueue();
return;
}
String servedName = queueNames[front];
String servedPlate = queuePlates[front];
bool hasNext = (rear - front) > 1;
String nextName = "";
String nextPlate = "";
String nextPhone = "";
if (hasNext) {
nextName = queueNames[front + 1];
nextPlate = queuePlates[front + 1];
nextPhone = queuePhones[front + 1];
nextPhone.trim();
Serial.print("[EXIT] Next rider: "); Serial.print(nextName);
Serial.print(" | Phone: '"); Serial.print(nextPhone); Serial.println("'");
}
for (int i = front; i < rear - 1; i++) {
queueNames[i] = queueNames[i + 1];
queuePlates[i] = queuePlates[i + 1];
queuePhones[i] = queuePhones[i + 1];
memcpy(queueUIDs[i], queueUIDs[i + 1], 4);
}
rear--;
lcd.clear();
lcd.setCursor(0, 0); lcd.print(servedName.substring(0, 16));
lcd.setCursor(0, 1); lcd.print("Amehudumia");
delay(2000); 38
if (hasNext) {
if (nextPhone.length() > 6) {
sendSMS(nextPhone,
"Ndugu " + nextName + " (" + nextPlate + "), ni zamu yako kupakiza 
abiria. "
);
} else {
Serial.println("[EXIT] WARNING: Next rider phone empty — turn SMS 
skipped.");
}
}
showQueue();
}
void checkAndInjectDefaults() {
}
