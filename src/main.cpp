#include <Arduino.h>

#include <NTPClient.h>
#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <WiFiUdp.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

#include <Fonts/FreeMono12pt7b.h>
const GFXfont *font = &FreeMono12pt7b;
const uint8_t xAdv  = 22;
const uint8_t yAdv  = 24;

/*  font                 xAdvance  yAdvance
    
    FreeMono9pt7b        11        18
    FreeMonoBold9pt7b    11        18
    FreeMono12pt7b       14        24
    FreeMonoBold12pt7b   14        24
    FreeMono18pt7b       21        35
    FreeMonoBold18pt7b   21        35
    FreeMono24pt7b       28        47
    FreeMonoBold24pt7b   28        47
    
    FreeSans9pt7b        5..18     22
    FreeSansBold9pt7b    5..18     22
    FreeSans12pt7b       6..24     29
    FreeSans18pt7b       9..36     42
    FreeSans24pt7b       12..48    56
    
    FreeSerif9pt7b       5..17     22           */

// ST7789 TFT module connections
#define TFT_DC    D1     // TFT DC  pin is connected to NodeMCU pin D1 (GPIO5)
#define TFT_RST   D2     // TFT RST pin is connected to NodeMCU pin D2 (GPIO4)
#define TFT_CS    D8     // TFT CS  pin is connected to NodeMCU pin D8 (GPIO15)

// For 1.14", 1.3", 1.54", and 2.0" TFT with ST7789:
Adafruit_ST7789  tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

WiFiManager wifiManager;
WiFiUDP     ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60*60*1000);

int boot = true;
volatile int update_display = true;

char *line[] = {
  "ITLISASAMPM",
  "ACQUARTERDC",
  "TWENTYFIVEX",
  "HALFSTENFTO",
  "PASTERUNINE",
  "ONESIXTHREE",
  "FOURFIVETWO",
  "EIGHTELEVEN",
  "SEVENTWELVE",
  "TENSEOCLOCK"
};

// Some ready-made 16-bit ('565') color settings:
#define ST77XX_BLACK    0x0000
#define ST77XX_GRAY     0x8410
#define ST77XX_WHITE    0xFFFF
#define ST77XX_RED      0xF800
#ifndef ST77XX_ORANGE
#define ST77XX_ORANGE   0xFA60
#endif
#define ST77XX_YELLOW   0xFFE0 
#define ST77XX_LIME     0x07FF
#define ST77XX_GREEN    0x07E0
#define ST77XX_CYAN     0x07FF
#define ST77XX_AQUA     0x04FF
#define ST77XX_BLUE     0x001F
#define ST77XX_MAGENTA  0xF81F
#define ST77XX_PINK     0xF8FF

word ConvertRGB( byte R, byte G, byte B) {
  return ( ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3) );
}

//const word low_color  = 0x8410;
const word low_color  = ConvertRGB(25,25,25);

//const word high_color = ST77XX_GREEN;
//const word high_color = ST77XX_CYAN;
const word high_color = ConvertRGB(173,194,0); // rindus

void ICACHE_RAM_ATTR inline ISR_timer0() {
  update_display = true;
  timer0_write(ESP.getCycleCount() + (20*80000000L)); // 20*80M/80M = 20*1s = 20s
}

void configModeCallback (WiFiManager *myWiFiManager) {
  if(boot) {
    tft.setTextColor(ST77XX_RED);
    tft.print("AP");
    tft.setTextColor(ST77XX_WHITE);
    tft.print("...");
  }
}

void setColor(int s_line, int s_char, int e_char, word color) {
  for(;s_char<=e_char;s_char++) {
    tft.drawChar(4+(s_char*xAdv), (s_line*yAdv)-8, line[s_line-1][s_char], color, 0x0000, 1);
    delay(80);
  }
}

void clockface() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(font);

  for(int l=1;l<=10;l++) {
    for(int c=0;c<11;c++) {
      tft.drawChar(4+(c*xAdv), (l*yAdv)-8, line[l-1][c], low_color, 0x0000, 1);
    }
  }

  for(int l=1;l<=10;l++) {
    for(int c=0;c<11;c++) {
      tft.drawChar(4+(c*xAdv), (l*yAdv)-8, line[l-1][c], low_color, 0x0000, 1);
    }
  }

  setColor(1,0,1,high_color); // IT
  setColor(1,3,4,high_color); // IS
}

void setup(void) {
  pinMode(D0,          OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  digitalWrite(D0,          0); // TFT Backlight off
  digitalWrite(LED_BUILTIN, 1); // alarm LED off

  Serial.begin(115200);

  // if the display has CS pin try with SPI_MODE0
  tft.init(240, 240, SPI_MODE2);    // Init ST7789 display 240x240 pixel

  // if the screen is flipped, remove this command
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  digitalWrite(D0, 1); // TFT Backlight on

  tft.println("   #################################");
  tft.println("   #                               #");
  tft.print("   #  WordClock by ");
  tft.setTextColor(high_color);
  tft.print("rindus");
  tft.setTextColor(ST77XX_WHITE);
  tft.println(" IoT Jam  #");
  tft.println("   #                               #");
  tft.println("   #################################");
  tft.println("");

  noInterrupts();
    // interupt setup
    timer0_isr_init();
    timer0_attachInterrupt(ISR_timer0);
    timer0_write(ESP.getCycleCount() + 2* 80000000L); // start in 2s (2*80M/80M=2s)
  interrupts();

  tft.print("WiFi...");
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setDebugOutput(false);
  wifiManager.autoConnect("QlockTwo");
  tft.setTextColor(ST77XX_GREEN);
  tft.println("done");
  tft.setTextColor(ST77XX_WHITE);
  
  WiFi.mode(WIFI_STA); // Ensure AP is off  

  if (WiFi.status() == WL_CONNECTED) {
    char buf[9];
    tft.print("NTP...");
    timeClient.begin();
    timeClient.forceUpdate();
    timeClient.getFormattedTime().toCharArray(buf, 9);
    buf[5]=0; // remove :ss
    tft.setTextColor(ST77XX_GREEN);
    tft.println(buf);
    tft.setTextColor(ST77XX_WHITE);
  }
  tft.println("done");
  delay(2500);
  clockface();
  boot = false;
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
  } else {
    digitalWrite(LED_BUILTIN, 0); // alarm LED on
    Serial.println("ERROR: WiFi not connected!");
    delay(2);
    digitalWrite(LED_BUILTIN, 1); // alarm LED off
    delay(498);
  }

  if( update_display ) {
    update_display = false;

    int minutes = timeClient.getMinutes();
    minutes = minutes - (minutes % 5);

    int hours = timeClient.getHours();
    if(hours>12)    { hours = hours-12; }
    if(minutes>=35) { ++hours; }

    Serial.print(hours, DEC);
    Serial.print(":");
    Serial.println(minutes, DEC);

    if(minutes==5) {
      setColor(10,5,10,low_color);  // oclock
      setColor(3,6,9,high_color);   // FIVE
    }
    if(minutes==10) {
      setColor(3,6,9,low_color);    // five
      setColor(4,5,7,high_color);   // TEN
    }
    if(minutes==15) {
      setColor(4,5,7,low_color);    // ten
      setColor(2,0,0,high_color);   // A
      setColor(2,2,8,high_color);   // QUARTER
    }
    if(minutes==20) {
      setColor(2,0,0,low_color);    // a
      setColor(2,2,8,low_color);    // quarter
      setColor(3,0,5,high_color);   // TWENTY
    }
    if(minutes==25) {
      //setColor(3,0,5,low_color);    // twenty
      setColor(3,0,9,high_color);   // TWENTYFIVE
    }
    if(minutes==30) {
      setColor(3,0,9,low_color);    // twentyfive
      setColor(4,0,3,high_color);   // HALF
    }
    if(minutes==35) {
      setColor(4,0,3,low_color);    // half
      setColor(3,0,9,high_color);   // TWENTYFIVE
    }
    if(minutes==40) {
      setColor(3,6,9,low_color);    //       five
      setColor(3,0,5,high_color);   // TWENTY
    }
    if(minutes==45) {
      setColor(3,0,5,low_color);    // twenty
      setColor(2,0,0,high_color);   // A
      setColor(2,2,8,high_color);   // QUARTER
    }
    if(minutes==50) {
      setColor(2,0,0,low_color);    // a
      setColor(2,2,8,low_color);    // quarter
      setColor(4,5,7,high_color);   // TEN
    }
    if(minutes==55) {
      setColor(4,5,7,low_color);    // ten
      setColor(3,6,9,high_color);   // FIVE
    }

    if(minutes==0||minutes==60) {
      setColor(3,6,9,low_color);    // five
      setColor(4,9,10,low_color);   // to
      setColor(5,0,3,low_color);    // past
      setColor(10,5,10,high_color); // OCLOCK
    } else {
      if(minutes>30) {
        setColor(5,0,3,low_color);   // past
        setColor(4,9,10,high_color); // TO
      } else {
        setColor(10,5,10,low_color); // oclock
        setColor(5,0,3,high_color);  // PAST
      }
    }
    
    if(hours==1) {
      setColor(9,5,10,low_color);   // twelve
      setColor(6,0,2,high_color);   // ONE
    }
    if(hours==2) {
      setColor(6,0,2,low_color);    // one
      setColor(7,8,10,high_color);  // TWO
    }
    if(hours==3) {
      setColor(7,8,10,low_color);   // two
      setColor(6,6,10,high_color);  // THREE
    }
    if(hours==4) {
      setColor(6,6,10,low_color);   // three
      setColor(7,0,3,high_color);   // FOUR
    }
    if(hours==5) {
      setColor(7,0,3,low_color);    // four
      setColor(7,4,7,high_color);   // FIVE
    }
    if(hours==6) {
      setColor(7,4,7,low_color);    // five
      setColor(6,3,5,high_color);   // SIX
    }
    if(hours==7) {
      setColor(6,3,5,low_color);    // six
      setColor(9,0,4,high_color);   // SEVEN
    }
    if(hours==8) {
      setColor(9,0,4,low_color);    // seven
      setColor(8,0,4,high_color);   // EIGHT
    }
    if(hours==9) {
      setColor(8,0,4,low_color);    // eight
      setColor(5,7,10,high_color);  // NINE
    }
    if(hours==10) {
      setColor(5,7,10,low_color);   // nine
      setColor(10,0,2,high_color);  // TEN
    }
    if(hours==11) {
      setColor(10,0,2,low_color);   // ten
      setColor(8,5,10,high_color);  // ELEVEN
    }
    if((hours==0)|(hours==12)) {
      setColor(8,5,10,low_color);   // eleven
      setColor(9,5,10,high_color);  // TWELVE
    }
  }
}