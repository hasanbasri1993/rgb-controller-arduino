#define REDUCED_MODES // sketch too big for Arduino Leonardo flash, so invoke reduced modes
#include <WS2812FX.h>
#include <ArduinoJson.h> // use ArduinoJson v5. ArduinoJson v6 will not work.
#include <EEPROM.h>

#define LED_COUNT 30
#define LED_PIN 7
#define MAX_NUM_CHARS 500 // maximum number of characters read from the Serial Monitor

char cmd[MAX_NUM_CHARS];      // char[] to store incoming serial commands
boolean cmd_complete = false; // whether the command string is complete
boolean debug = false;        // whether the command string is complete
String command;
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Starting Up ... !!");

  // init LED strip with a default segment
  ws2812fx.init();
  ws2812fx.setBrightness(127);
  // parameters: seg index, start led, stop led, mode, color, speed, reverse
  ws2812fx.setSegment(0, 0, LED_COUNT - 1, FX_MODE_SCAN, RED, 1000, false);
  // if segment data had been previously saved to eeprom, load that data
  restoreFromEEPROM();
  ws2812fx.start();
}

void loop()
{
  ws2812fx.service();
  recvChar();
}

void recvChar()
{
  static byte index = 0;
  while (Serial.available() > 0 && cmd_complete == false)
  {
    String data = Serial.readString();
    data = data.substring(1, data.length() - 1); // remove the surrounding quotes
    data.replace("\\\"", "\"");
    DynamicJsonBuffer jsonBuffer(1024);
    JsonObject &root = jsonBuffer.parseObject(data);

    if (debug)
    {
      Serial.println(data);
    }

    String command = root["command"];
    Serial.println(command);
    if (!cmd_complete)
    {

      //"{\"command\":\"debug_on\"}"
      if (command == "debug_on")
      {
        debug = true;
        Serial.println("debug_on");
      }

      //{"command":"debug_off"}
      if (command == "debug_off")
      {
        debug = false;
        Serial.println("debug_off");
      }

      //{"command":"setsegments","numPixels":30,"brightness":255,"segments":[{"start":0,"stop":4,"mode":1,"speed":2000,"options":0,"colors":[16711680,65280,255]},{"start":5,"stop":9,"mode":5,"speed":170,"options":0,"colors":[16711680,65280,255]},{"start":10,"stop":23,"mode":35,"speed":170,"options":0,"colors":[16711680,65280,255]},{"start":24,"stop":29,"mode":1,"speed":170,"options":0,"colors":[16711680,65280,255]}]}
      //"{\"command\":\"setsegments\"\"numPixels\":30,\"brightness\":255,\"segments\":[{\"start\":0,\"stop\":29,\"mode\":10,\"speed\":1000,\"options\":0,\"colors\":[16711680,65280,255]}]}"
      if (command == "setsegments")
      {
        ws2812fx.stop();
        ws2812fx.setLength(root["numPixels"]);
        ws2812fx.stop(); // reset strip again in case length was increased
        ws2812fx.setBrightness(root["brightness"]);
        ws2812fx.setNumSegments(1); // reset number of segments
        JsonArray &segments = root["segments"];
        for (int i = 0; i < segments.size(); i++)
        {
          JsonObject &seg = segments[i];
          JsonArray &colors = seg["colors"];
          // the web interface sends three color values
          uint32_t _colors[NUM_COLORS] = {colors[0], colors[1], colors[2]};
          uint8_t _options = seg["options"];
          ws2812fx.setSegment(i, seg["start"], seg["stop"], seg["mode"], _colors, seg["speed"], _options);
        }
        saveToEEPROM(); // save segment data to EEPROM
        ws2812fx.start();
        Serial.println("Setsegment - Saved");
      }

      //{"command":"stop"}
      if (command == "stop")
      {
        ws2812fx.stop();
        Serial.println("OFF");
      }

      //{"command":"start"}
      if (command == "start")
      {
        ws2812fx.start();
        Serial.println("ON");
      }

      //{"command":"pause"}
      if (command == "pause")
      {
        ws2812fx.pause();
        Serial.println("PAUSE");
      }

      //{"command":"resume"}
      if (command == "resume")
      {
        ws2812fx.resume();
        Serial.println("RESUME");
      }

      //
      //{"command":"getsegments"}
      if (command == "getsegments")
      {
        JsonObject &root = jsonBuffer.createObject();
        root["pin"] = ws2812fx.getPin();
        root["numPixels"] = ws2812fx.numPixels();
        root["brightness"] = ws2812fx.getBrightness();
        root["numSegments"] = ws2812fx.getNumSegments();
        JsonArray &jsonSegments = root.createNestedArray("segments");

        WS2812FX::segment *segments = ws2812fx.getSegments();
        for (int i = 0; i < ws2812fx.getNumSegments(); i++)
        {
          WS2812FX::segment seg = segments[i];
          JsonObject &jsonSegment = jsonBuffer.createObject();
          jsonSegment["start"] = seg.start;
          jsonSegment["stop"] = seg.stop;
          jsonSegment["mode"] = seg.mode;
          jsonSegment["speed"] = seg.speed;
          jsonSegment["options"] = seg.options;
          JsonArray &jsonColors = jsonSegment.createNestedArray("colors");
          jsonColors.add(seg.colors[0]); // the web interface expects three color values
          jsonColors.add(seg.colors[1]);
          jsonColors.add(seg.colors[2]);
          jsonSegments.add(jsonSegment);
        }
        root.printTo(Serial);
        int bufferSize = root.measureLength() + 1;
        char *json = (char *)malloc(sizeof(char) * (bufferSize));
        root.printTo(json, sizeof(char) * bufferSize);
        Serial.println(json);
        free(json);
      }
    }
  }
}

#define EEPROM_MAGIC_NUMBER 0x010e0d05
void saveToEEPROM()
{
  WS2812FX::segment tmpSegments[MAX_NUM_SEGMENTS]; // tmp space for segment data
  Serial.println("saving to EEPROM");
  EEPROM.put(sizeof(int) * 0, (int)EEPROM_MAGIC_NUMBER);
  EEPROM.put(sizeof(int) * 1, (int)ws2812fx.getPin());
  EEPROM.put(sizeof(int) * 2, (int)ws2812fx.numPixels());
  EEPROM.put(sizeof(int) * 3, (int)ws2812fx.getBrightness());
  EEPROM.put(sizeof(int) * 4, (int)ws2812fx.getNumSegments());
  memcpy(&tmpSegments, ws2812fx.getSegments(), sizeof(tmpSegments));
  EEPROM.put(sizeof(int) * 5, tmpSegments);
  //EEPROM.commit(); // for ESP8266 (comment out if using an Arduino)
}

void restoreFromEEPROM()
{
  Serial.println("loading from EEPROM");
  int magicNumber = 0;
  int pin;
  int length;
  int brightness;
  int numSegments;
  WS2812FX::segment tmpSegments[MAX_NUM_SEGMENTS]; // tmp space for segment data
  EEPROM.get(sizeof(int) * 0, magicNumber);
  Serial.println("restoring from EEPROM");
  EEPROM.get(sizeof(int) * 1, pin);
  ws2812fx.setPin(pin);
  EEPROM.get(sizeof(int) * 2, length);
  ws2812fx.setLength(length);
  EEPROM.get(sizeof(int) * 3, brightness);
  ws2812fx.setBrightness(brightness);
  EEPROM.get(sizeof(int) * 4, numSegments);
  ws2812fx.setNumSegments(numSegments);
  EEPROM.get(sizeof(int) * 5, tmpSegments);
  memcpy(ws2812fx.getSegments(), &tmpSegments, sizeof(tmpSegments));
}
