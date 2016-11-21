#include <WiFi101.h>
#include <Adafruit_NeoPixel.h>
///https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define PIN 6 //DATA pin of the neopixels on the arduino
#define LEDS 24 // this is a 24 neopixels ring
char ssid[] = "manouland"; //  your network SSID (name)
char pass[] = "manoumanou";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS;

const unsigned long HTTP_TIMEOUT = 10000;  // max respone time from server
const size_t MAX_CONTENT_SIZE = 512;       // max size of the HTTP response
const char* OCTOPI_APIKEY = "7758E1DAA337489FBE81B209CD125713"; //OCTOPI API key
const char* resource = "/api/printer?exclude=sd";                    // http resource
//char* json[MAX_CONTENT_SIZE]; //json char array to process
  
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char server[] = "192.168.1.122";    // OCTOPI server IP address

WiFiClient client;

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDS, PIN, NEO_GRB + NEO_KHZ800);

int maxpixpertool=(strip.numPixels())/2;  //we split the 24 ring in two, one half for BED, other half for HOT END
int startTemp=30;                         //no lights before this temp on sensors
int maxBedTemp=90;                        //theorical max bed temp, to do comparisons with actual temp
int maxToolTemp=240;                      //theorical max hot end temp, to do comparisons with actual temp

// The type of data that we want to extract from the Octopi REST API page
struct PrinterData {
  char error[32] ;
  char printing[32] ;
  char actualbed[6];
  char targetbed[6];
  char actualtool0[6];
  char targettool0[6];
};


void setup() {
  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
  #if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif
  // End of trinket special code
  
  //Initialize serial and wait for port to open:
  Serial.begin(9600);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    theaterChase(strip.Color(255, 255, 255), 50); // BLINK white while connecting
    // wait some seconds for connection:
    //delay(250);
  }
  Serial.println("Connected to wifi");
  theaterChase(strip.Color(0, 0, 255), 50); // BLINK blue when connected
  printWifiStatus();


}

void loop() {
  //each loop we get the JSON and process it
  //the real work is done in printUserData()
  if (sendRequest(server, resource, OCTOPI_APIKEY) && skipResponseHeaders()) {
    char response[MAX_CONTENT_SIZE];
    readReponseContent(response, sizeof(response));
  
    PrinterData printerData;
    if (parseUserData(response, &printerData)) {
      printUserData(&printerData);
    }
  }

//  rainbow(20);
//  rainbowCycle(20);
//  theaterChaseRainbow(50);

delay(50);
}

// Skip HTTP headers so that we are at the beginning of the response's body
bool skipResponseHeaders() {
  // HTTP headers end with an empty line
  char endOfHeaders[] = "\r\n\r\n";
  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);

  if (!ok) {
    Serial.println("No response or invalid response!");
  }

  return ok;
}

// Read the body of the response from the HTTP server
void readReponseContent(char* content, size_t maxSize) {
  size_t length = client.readBytes(content, maxSize);
  content[length] = 0;
  Serial.println(content);
}

bool parseUserData(char* content, struct PrinterData* printerData) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(content);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return false;
  }

  // Here were copy the strings we're interested in
  strcpy(printerData->error, root["state"]["flags"]["error"]);
  strcpy(printerData->printing, root["state"]["flags"]["printing"]);
  strcpy(printerData->actualbed, root["temperature"]["bed"]["actual"]);
  strcpy(printerData->targetbed, root["temperature"]["bed"]["target"]);
  strcpy(printerData->actualtool0, root["temperature"]["tool0"]["actual"]);
  strcpy(printerData->targettool0, root["temperature"]["tool0"]["target"]);

  return true;
}

// Print the data extracted from the JSON
void printUserData(const struct PrinterData* printerData) {
  //convert char to float for calculations
  float actualbed = atof(printerData->actualbed);  
  float targetbed = atof(printerData->targetbed);
  float actualtool0 = atof(printerData->actualtool0);
  float targettool0 = atof(printerData->targettool0);

  //SERIAL debug
  Serial.print("error = ");
  Serial.println(printerData->error);
  Serial.print("printing = ");
  Serial.println(printerData->printing);
  Serial.print("actualbed = ");
  Serial.println(actualbed);
  Serial.print("targetbed = ");
  Serial.println(targetbed);
  Serial.print("actualtool0 = ");
  Serial.println(actualtool0);
  Serial.print("targettool0 = ");
  Serial.println(targettool0);

  //calculate intensity to make the neopixels glow according to the temp, relative to constants start/max
  //we make this 127 max, so that the progress bar will show up on top with 255
  int intensityBed=constrain(map(actualbed,startTemp,maxBedTemp,0,127),0,127);
  int intensityTool=constrain(map(actualtool0,startTemp,maxToolTemp,0,127),0,127);
  Serial.print("intensityBed = ");
  Serial.println(intensityBed);
  Serial.print("intensityTool = ");
  Serial.println(intensityTool);
  
  //progress bar constuction for bed temp, by default we take out the 12 neopixels
  int pixBedOff=12;
  //if we have a target temp for bed, adjust the number of neopixels that should be shut off during the heating
  if (targetbed>0) pixBedOff=maxpixpertool-constrain(map(actualbed,startTemp,targetbed,0,maxpixpertool),0,maxpixpertool);
  Serial.print("pixBedOff = ");
  Serial.println(pixBedOff);

  //progress bar constuction for hot end temp, by default we take out the 12 neopixels
  int pixToolOff=12;
  //if we have a target temp for hot end, adjust the number of neopixels that should be shut off during the heating
  if (targettool0>0) pixToolOff=maxpixpertool-constrain(map(actualtool0,startTemp,targettool0,0,maxpixpertool),0,maxpixpertool);
  Serial.print("pixToolOff = ");
  Serial.println(pixToolOff);


  if (strcmp(printerData->printing, "true")  == 0) {  //printing is ongoing, all lights ON white !
      colorWipe(strip.Color(255, 255, 255), 20,true,0); 
      colorWipe(strip.Color(255, 255, 255), 20,false,0); 
  } else {  //not printing is idling (cooldown) or heating up, so we adjust the neopixels
      colorWipe(strip.Color(intensityBed, 0, 0), 10,true,0); //init all BED neopixels RED, progressive
      colorWipe(strip.Color(intensityTool, intensityTool, 0), 10,false,0); //init all HOT END neopixels YELLOW, progressive
      colorWipe(strip.Color(255, 0, 0), 10,true,pixBedOff); //progress bar for BED, full ON
      colorWipe(strip.Color(255, 255, 0), 10,false,pixToolOff); //progress bar for HOT END, full ON
  }
}

// Send the HTTP GET request to the server
bool sendRequest(const char* host, const char* resource, const char* apikey) {
  Serial.print("GET ");
  Serial.println(resource);

  if (client.connect(host, 80)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    client.print("GET ");
    client.print(resource);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(host);
    client.print("X-Api-Key: ");
    client.println(OCTOPI_APIKEY);
    client.println("Connection: close");
    client.println();
  }

  return true;
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// Fill the dots one after the other with a color
//modified Adafruit colorWipe to handle BED/HOTEND and progress bar
void colorWipe(uint32_t c, uint8_t wait, bool tool, uint8_t onpix) {
 if (tool) {  // tool==true, this is BED
    for(uint16_t i=0; i<((strip.numPixels())/2)-onpix; i++) { //first neopixel to 11th, minus progres bar
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
    }
  } else {// tool==false, this is HOT END
    for(uint16_t i=(strip.numPixels())/2; i<(strip.numPixels())-onpix; i++) { //12th neopixel to 24th, minus progres bar
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
    }    
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
