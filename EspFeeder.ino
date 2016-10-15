/*
 EspFeeder -- Web Server Enabled Pet Feeder
 Base code is FSWEbServer example, as shown below.
 Code is highly modifed.
*/


/*
  FSWebServer - Example WebServer with SPIFFS backend for esp8266
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" esp8266fs.local/edit; done

  access the sample web page at http://esp8266fs.local
  edit the page by going to http://esp8266fs.local/edit
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <FS.h>

#include <ArduinoJson.h>
#include <Servo.h>
#include <Bounce2.h>
#include <string.h>

#define BUTTON1 2
#define SERVO1 13
#define BUTTON2 14
#define SERVO2 12
#define TONE 4


Servo servo1;
Servo servo2;

Bounce b1 = Bounce();
Bounce b2 = Bounce();


#define DBG_OUTPUT_PORT Serial

const char* configFile = "/config.json";

char ssid[31] = { "SSID" };
char password[31] = { "PASSWORD" };
char host[31] = { "EspFeeder2" };

int offset = -18000;
int s1state = true;
int s1min = 10;
int s1max = 90;
int s2state = true;
int s2min = 10;
int s2max = 90;
char s1time[10];
int s1hour = 6;
int s1minute = 0;
char s2time[10];
int s2hour = 18;
int s2minute = 0;

int apMode = false;

ESP8266WebServer server(80);
//holds the current upload
File fsUploadFile;

IPAddress timeServerIP;
char timeServer[31] = { "0.pool.ntp.org" };
char gettime[10] = { "02:01" };
int gethour = 2;
int getminute = 1;
char resettime[10] = { "00:00" };
int resethour = 0;
int resetminute = 0;
unsigned int localPort = 2390; // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

time_t timeNow = 0;
time_t ms = 0;
int lastminute = -1;
int gettingNtp = false;
int flagRestart = false;

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.print(timeNow);
  DBG_OUTPUT_PORT.println(" handleFileRead: " + path);
  if(path.endsWith("/")) path += apMode?"setup.htm":"index.htm";

  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
    if (upload.filename == configFile)
    {
      loadConfig();
      setServos();
    }
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}

  String path = server.arg("dir");
  DBG_OUTPUT_PORT.print(timeNow);
  DBG_OUTPUT_PORT.println(" handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

void loadConfig()
{
  if(SPIFFS.exists(configFile))
  {
    File file = SPIFFS.open(configFile, "r");
    char json[500];
    memset(json,0,sizeof(json));
    file.readBytes(json,sizeof(json));
    file.close();
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    if (!root.success()) {
        DBG_OUTPUT_PORT.println("json parse of configFile failed.");
    }
    else
    {
      if (root.containsKey("ssid")) strncpy(ssid,root["ssid"],30);
      if (root.containsKey("password")) strncpy(password,root["password"],30);
      if (root.containsKey("host")) strncpy(host,root["host"],30);
      if (root.containsKey("timeserver")) strncpy(timeServer,root["timeserver"],30);
      if (root.containsKey("s1min")) s1min = root["s1min"];
      if (root.containsKey("s1max")) s1max = root["s1max"];
      if (root.containsKey("s2min")) s2min = root["s2min"];
      if (root.containsKey("s2max")) s2max = root["s2max"];
      if (root.containsKey("s1time")) strncpy(s1time,root["s1time"],10);
      if (root.containsKey("s2time")) strncpy(s2time,root["s2time"],10);
      if (root.containsKey("gettime")) strncpy(gettime,root["gettime"],10);
      if (root.containsKey("resettime")) strncpy(resettime,root["resettime"],10);

      s1hour = atoi(s1time);
      if (strchr(s1time,':')) s1minute = atoi(strchr(s1time,':')+1);
      s2hour = atoi(s2time);
      if (strchr(s2time,':')) s2minute = atoi(strchr(s2time,':')+1);

      gethour = atoi(gettime);
      if (strchr(gettime,':')) getminute = atoi(strchr(gettime,':')+1);
      resethour = atoi(resettime);
      if (strchr(resettime,':')) resetminute = atoi(strchr(resettime,':')+1);

      DBG_OUTPUT_PORT.printf("Config: host: %s ssid: %s timeserver: %s\n",host,ssid,timeServer);
      DBG_OUTPUT_PORT.printf("s1time: %s %d %d s2time:%s %d %d s1:%d %d s2:%d %d\n",
        s1time, s1hour, s1minute, s2time, s2hour, s2minute, s1min, s1max, s2min, s2max);
      DBG_OUTPUT_PORT.printf("gettime: %s %d %d resettime:%s %d %d\n",
        gettime, gethour, getminute, resettime, resethour, resetminute);

    }
  }
  else
  {
    DBG_OUTPUT_PORT.printf("config file: %s not found\n",configFile);
  }
}


void setServos()
{
  int s1 = s1state?s1min:s1max;
  int s2 = s2state?s2min:s2max;
  servo1.write(s1);
  servo2.write(s2);
  DBG_OUTPUT_PORT.printf("set servo1=%d servo2=%d\n",s1,s2);
}

void getNTP()
{

  if (gettingNtp) return;
  time_t failms = millis();
  time_t ims = millis();
  int tries = 0;
  gettingNtp = true;
  while(gettingNtp)
  {
    tries++;
    if (timeNow > 100) // we have successfully got the time before
      if ((millis() - failms) > 60*1000 ) // 1 minutes
        {
          gettingNtp = false;
          return;  // lets just foreget it
        }
    if ((millis() - failms) > 5*60*1000) // 5 minutes
    {
      ESP.restart();
    }
    if (timeServerIP == INADDR_NONE || (tries % 3) == 1)
    {
      //get a random server from the pool
      DBG_OUTPUT_PORT.print("\nLooking up:");
      DBG_OUTPUT_PORT.println(timeServer);

      WiFi.hostByName(timeServer, timeServerIP);
      DBG_OUTPUT_PORT.print("Timeserver IP:");
      DBG_OUTPUT_PORT.println(timeServerIP);

      if (timeServerIP == INADDR_NONE)
      {
        DBG_OUTPUT_PORT.println("bad IP, try again");
        delay(1000);
        tone(TONE,500,50);
        continue;
      }
    }
    tone(TONE,500,50);
    DBG_OUTPUT_PORT.println("sending NTP packet...");
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();

    ims = millis();
    while(gettingNtp)
    {
      if ((millis() - ims) > 5000) break; // if > 15 seconds waiting for packet, send packet again (break into outer loop)
      // wait to see if a reply is available
      delay(1000);

      int cb = udp.parsePacket();
      if (!cb) {
        DBG_OUTPUT_PORT.print(".");
      }
      else {
        DBG_OUTPUT_PORT.print("packet received, length=");
        DBG_OUTPUT_PORT.println(cb);
        // We've received a packet, read the data from it
        udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

        //the timestamp starts at byte 40 of the received packet and is four bytes,
        // or two words, long. First, esxtract the two words:

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        // combine the four bytes (two words) into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        DBG_OUTPUT_PORT.print("Seconds since Jan 1 1900 = " );
        DBG_OUTPUT_PORT.println(secsSince1900);

        // now convert NTP time into everyday time:
        DBG_OUTPUT_PORT.print("Unix time = ");
        // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
        const unsigned long seventyYears = 2208988800UL;
        // subtract seventy years:
        unsigned long epoch = secsSince1900 - seventyYears;
        // print Unix time:
        DBG_OUTPUT_PORT.println(epoch);


        // print the hour, minute and second:
        DBG_OUTPUT_PORT.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
        DBG_OUTPUT_PORT.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
        DBG_OUTPUT_PORT.print(':');
        if ( ((epoch % 3600) / 60) < 10 ) {
          // In the first 10 minutes of each hour, we'll want a leading '0'
          DBG_OUTPUT_PORT.print('0');
        }
        DBG_OUTPUT_PORT.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
        DBG_OUTPUT_PORT.print(':');
        if ( (epoch % 60) < 10 ) {
          // In the first 10 seconds of each minute, we'll want a leading '0'
          DBG_OUTPUT_PORT.print('0');
        }
        DBG_OUTPUT_PORT.println(epoch % 60); // print the second
        timeNow = epoch;
        gettingNtp = false;
      }
    }
  }


}

void setup(void){


  apMode = false;
  DBG_OUTPUT_PORT.begin(74880);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);



  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }

  loadConfig();

  //WIFI INIT

  WiFi.mode(WIFI_STA);

  time_t wifims = millis();

  DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
    tone(TONE,300,50);
    if ((millis() - wifims) > 60 * 1000) // 60 seconds of no wifi connect
    {
      break;
    }
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    DBG_OUTPUT_PORT.println("\ngoing to AP mode ");
    delay(500);
    WiFi.mode(WIFI_AP);
    delay(500);
    apMode = true;
    uint8_t mac[6];
    delay(500);
    WiFi.softAPmacAddress(mac);
    delay(500);
    sprintf(ssid,"EspFeeder_%02x%02x%02x",mac[3],mac[4],mac[5]);
    DBG_OUTPUT_PORT.print("SoftAP ssid:");
    DBG_OUTPUT_PORT.println(ssid);
    WiFi.softAP(ssid);
    delay(1000);
    tone(TONE,1000,50);
    delay(50);
    tone(TONE,500,50);
    delay(50);
    tone(TONE,300,50);
    DBG_OUTPUT_PORT.println("");
    DBG_OUTPUT_PORT.print("AP mode. IP address: ");
    DBG_OUTPUT_PORT.println(WiFi.softAPIP());
  }
  else
  {
    tone(TONE,300,50);
    delay(50);
    tone(TONE,500,50);
    delay(50);
    tone(TONE,1000,1000);
    DBG_OUTPUT_PORT.println("");
    DBG_OUTPUT_PORT.print("Connected! IP address: ");
    DBG_OUTPUT_PORT.println(WiFi.localIP());
  }

  MDNS.begin(host);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(host);
  DBG_OUTPUT_PORT.print(".local/ or http://");
  if (apMode)
  {
    DBG_OUTPUT_PORT.print(WiFi.softAPIP());
  }
  else
  {
    DBG_OUTPUT_PORT.print(WiFi.localIP());
  }
  DBG_OUTPUT_PORT.println("/");

  // NTP init
  if (!apMode)
  {
    DBG_OUTPUT_PORT.println("Starting UDP for NTP");
    udp.begin(localPort);
    DBG_OUTPUT_PORT.print("Local port: ");
    DBG_OUTPUT_PORT.println(udp.localPort());

    delay(1000);

    getNTP();

  }
  servo1.attach(SERVO1);
  pinMode(BUTTON1,INPUT_PULLUP);
  b1.attach(BUTTON1);
  b1.interval(100);

  servo2.attach(SERVO2);
  pinMode(BUTTON2,INPUT_PULLUP);
  b2.attach(BUTTON2);
  b2.interval(100);

  setServos();

  pinMode(TONE,OUTPUT);

  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.on("/status", HTTP_GET, [](){
    String json = "{";
    json += String(  "\"s1\":\"")+String(s1state?"latched":"unlatched")+String("\"");
    json += String(", \"s2\":\"")+String(s2state?"latched":"unlatched")+String("\"");
    json += String(", \"time\":")+String(timeNow);
    json += "}";
    server.send(200, "text/json", json);
    DBG_OUTPUT_PORT.print("status ");
    DBG_OUTPUT_PORT.println(json);
    json = String();
  });

  server.on("/toggle1", HTTP_GET, [](){
    s1state = ! s1state;
    server.send(200, "text/text", "OK");
    DBG_OUTPUT_PORT.printf("toggle s1 = %d\n",s1state);
    setServos();
  });
  server.on("/toggle2", HTTP_GET, [](){
    s2state = ! s2state;
    server.send(200, "text/text", "OK");
    DBG_OUTPUT_PORT.printf("toggle s2 = %d\n",s2state);
    setServos();
  });
  server.on("/restart", HTTP_GET, [](){
    server.send(200, "text/text", apMode?"Stopping AP, Restarting... to connect to WiFi. Use your browser on your network to reconnect in a minute":"Restarting.... Wait a minute or so and then refresh.");
    delay(2000);
    ESP.restart();
  });

  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

}

void loop(void){

// deal with http server.
  server.handleClient();

// handle buttons
  b1.update();
  b2.update();

  if (b1.fell())
  {
    s1state = !s1state;
    setServos();
  }


  if (b2.fell())
  {
    s2state = !s2state;
    setServos();
  }

// keep track of the time
  if (!apMode && ms != millis())
  {
    ms = millis();
    if ( (ms % 1000) == 0)
    {
      timeNow++;
      time_t t = timeNow + offset;
      int hour = (t % 86400) / 3600;
      int minute = (t % 3600) / 60;
      if (lastminute != minute)
      {
        DBG_OUTPUT_PORT.printf("%d:%02d\n",hour,minute);
        lastminute = minute;
        if (flagRestart)
        {
          flagRestart = false;
          ESP.restart();
        }
        if (s1state)
        {
          if (s1hour == hour && s1minute == minute)
          {
            s1state = false;
            setServos();
          }
        }
        if (s2state)
        {
          if (s2hour == hour && s2minute == minute)
          {
            s2state = false;
            setServos();
          }
        }
        if (gethour == hour && getminute == minute)
        {
          if (!gettingNtp && !apMode) getNTP();
        }
        if (resethour == hour && resetminute == minute && resethour != 0 && resetminute != 0)
        {
          flagRestart = true;
        }
      }
    }
  }
}
