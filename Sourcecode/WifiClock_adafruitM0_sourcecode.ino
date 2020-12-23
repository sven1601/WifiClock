#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "TimeLib.h"
#include "RTClib.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "Adafruit_MCP9808.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include "Adafruit_SHT31.h"

#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2            

// Set to false to display time in 12 hour format, or true to use 24 hour:
#define TIME_24_HOUR      true
#define NTP_SYNC_INTERVAL 86400                                 /*M0 clock apparently drifts a bit.  re-sync every day */

Adafruit_8x16matrix matrix1 = Adafruit_8x16matrix();
Adafruit_8x16matrix matrix2 = Adafruit_8x16matrix();
Adafruit_8x16matrix matrix3 = Adafruit_8x16matrix();
Adafruit_8x16matrix matrix4 = Adafruit_8x16matrix();
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
Adafruit_SHT31 sht31 = Adafruit_SHT31();

int status = WL_IDLE_STATUS;
char ssid[] = "XXXXXXX";                                        // your network SSID (name)
char pass[] = "XXXXXXX";                                        // your network password
int keyIndex = 0;                                               // your network key Index number (needed only for WEP) 
unsigned int localPort = 2390;                                  // local port to listen for UDP packets 
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int sec_new = 0;
int sec_old = 0;
int min_new = 0;
int min_old = 0;
int hour_new = 0;
int hour_old = 0;
int year_new = 0;
int month_new = 0;
int day_new = 0;
float temp;
float humidity;
char global_buf[128];
unsigned char temp1 = 0;
unsigned char temp2 = 0;
unsigned char temp3 = 0;
unsigned char hum1 = 0;
unsigned char hum2 = 0;
unsigned char hum3 = 0;
unsigned int light_value = 0;
float tsl_value = 0.0;
int charcount=0;
char wecker_hour=0;
char wecker_min=0;
char *p;

// Statusvariablen
char power_status_new = 1;
char power_status_old = 1;

RTC_DS3231 rtc;
DateTime my_time_now;

/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
IPAddress timeServerIP;                                         // NTP server IP address
IPAddress own_ip(192, 168, 1, 170);                             // Eigene IP
IPAddress dns_ip(192, 168, 1, 1);                            
IPAddress gateway_ip(192, 168, 1, 1);                         
IPAddress subnet_ip(255, 255, 255, 0);                    


const char* ntpServerName = "0.de.pool.ntp.org";                //change to match your own regional pool

const int NTP_PACKET_SIZE = 48;                                 // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE];                             //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
WiFiServer server(80);
WiFiClient client;

//TSL2561 Light Sensor
sensor_t sensor;
sensors_event_t event;
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);


//Forward declarations
time_t getNTPTime();
unsigned long sendNTPpacket(IPAddress& address);
byte wifiConnect();
int ntpQuery();
void dot();
void set_disp();
float temp_read();
boolean summertime_RAMsave(int year, byte month, byte day, byte hour, byte tzHours);


void setup() {

  // Setup the WINC1500 connection with the pins above and the default hardware SPI.
  WiFi.setPins(WINC_CS, WINC_IRQ, WINC_RST);

  rtc.begin();
  tsl.begin();
  tempsensor.begin(); 
  sht31.begin(0x44);
                                                
  pinMode(WINC_EN, OUTPUT);
  digitalWrite(WINC_EN, HIGH);

  configureSensorTsl();
            
  matrix1.begin(0x70);  // pass in the address
  matrix2.begin(0x71);  // pass in the address
  matrix3.begin(0x72);  // pass in the address
  matrix4.begin(0x73);  // pass in the address

  matrix1.setBrightness(light_value);
  matrix2.setBrightness(light_value);
  matrix3.setBrightness(light_value);
  matrix4.setBrightness(light_value);

  matrix1.clear();
  matrix1.setCursor(3,0);
  matrix1.setRotation(3);
  matrix1.print("0");
  matrix1.writeDisplay();

  matrix2.clear();
  matrix2.setCursor(0,0);
  matrix2.setRotation(3);
  matrix2.print("0");
  matrix2.writeDisplay();

  matrix3.clear();
  matrix3.setCursor(3,0);
  matrix3.setRotation(3);
  matrix3.print("0");
  matrix3.writeDisplay();

  matrix4.clear();
  matrix4.setCursor(0,0);
  matrix4.setRotation(3);
  matrix4.print("0");
  matrix4.writeDisplay();

  if(wifiConnect() == 0)
  { 
    if(ntpQuery() == 0)
    {
      time_t timenow = now();                                                                                                       // Zeit von NTP Paket abfragen
      rtc.adjust(DateTime(year(timenow), month(timenow), day(timenow), hour(timenow), minute(timenow), second(timenow)));           // RTC Zeit setzen 
    }                                                      
  }

  my_time_now = rtc.now();

  set_disp(1, (unsigned char)(my_time_now.hour() / 10), 0);
  set_disp(2, (unsigned char)(my_time_now.hour() % 10), 0);
  set_disp(3, (unsigned char)(my_time_now.minute() / 10), 0);
  set_disp(4, (unsigned char)(my_time_now.minute() % 10), 0); 

  server.begin();

}

// Display dashboard page with on/off button for relay
// It also print Temperature in C and F
void dashboardPage(WiFiClient &client) {
  client.println("<!DOCTYPE HTML><html><head>");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body>");                                                             
  client.println("<h3>Arduino Web Server - <a href=\"/\">Neu Laden</a></h3>");

  client.println("<h3>Wecker:</h3>");

  client.println("<table>");
  
  client.println("<tr>");
  client.println("<td>Set time:</td>");
  client.println("<td>");
  client.println("<form action=\"/wecker_set.php\" method=\"get\">");
  client.println("<input type=\"text\" name=\"wecker_hour\" size=\"2\" value=\"\">:");
  client.println("<input type=\"text\" name=\"wecker_min\" size=\"2\" value=\"\">");
  client.println("<input type=\"submit\" value=\"Set\">");
  client.println("</form>");
  client.println("</td>");
  client.println("</tr>");
  
  client.println("<tr>");
  client.println("<td>Status:</td>");
  if(wecker_hour > 0 || wecker_min > 0)
  {
    sprintf(global_buf, "<td><h4>Alarm is set at %02i:%02i</h4><td>", wecker_hour, wecker_min);
    client.println(global_buf);
    client.println("<td><a href=\"/wecker_aus\"><button>Turn off</button></a></td>"); 
  }
  else
  {
    client.println("<td><h4>Alarm is OFF</h4></td>");    
  }
  client.println("</tr>");
  
  client.println("</table>");

  client.println("<h3>Licht:</h3>");
  
  client.println("<table>");
  
  client.println("<tr>");
  client.println("<td>Control:</td>");
  client.println("<td><a href=\"/power_on\"><button>Turn on</button></a></td>");
  client.println("<td><a href=\"/power_off\"><button>Turn off</button></a></td>");
  client.println("</tr>");

  client.println("<tr>");
  client.println("<td>Status:</td>");
  if(power_status_old) client.println("<td>ON</td>");
  else client.println("<td>OFF</td>");
  client.println("</tr>");
  
  client.println("</table>");  
  client.println("</body></html>"); 
}

void loop() {

  my_time_now = rtc.now();
  hour_new = my_time_now.hour();
  min_new = my_time_now.minute();
  sec_new = my_time_now.second();

  year_new = my_time_now.year();
  month_new = my_time_now.month();
  day_new = my_time_now.day();

  if(summertime_RAMsave(year_new, month_new, day_new, hour_new, 0))    
    hour_new += 2;
  else                  
    hour_new += 1;

  if(hour_new > 23)     hour_new -= 24;

  
    
  // Punkt toggeln...
  if(sec_new != sec_old)
  {
    temp = temp_read();
    
    temp1 = (unsigned char)((int)(temp)/10);
    temp2 = (unsigned char)((int)(temp)%10);
    temp3 = (unsigned char)(((int)(temp*10.0))%10);

    if(power_status_old)
    {    
      set_disp(1, (unsigned char)(hour_new / 10), temp1);
      set_disp(2, (unsigned char)(hour_new % 10), temp2);
  
      set_disp(3, (unsigned char)(min_new / 10), temp3);
      set_disp(4, (unsigned char)(min_new % 10), temp3); 
      
      dot(sec_new & 1);
  
      tsl.getEvent(&event);
      if (event.light)
      {
        tsl_value = event.light;
      }
  
      light_value = (int)(5.0 * tsl_value) / 45.0;
  
      matrix1.setBrightness(light_value);
      matrix2.setBrightness(light_value);
      matrix3.setBrightness(light_value);
      matrix4.setBrightness(light_value);
    }
   
    sec_old = sec_new;
  }

  if(power_status_old != power_status_new)
  {
    if(power_status_new == 0)
    {
      matrix1.clear();
      matrix2.clear();
      matrix3.clear();
      matrix4.clear();
  
      matrix1.writeDisplay();
      matrix2.writeDisplay();
      matrix3.writeDisplay();
      matrix4.writeDisplay();
    }
    power_status_old = power_status_new;  
  }

  // Ethernet Teil 

  client = server.available();
  if (client) 
  {
    Serial.println("new client");
    memset(global_buf,0,sizeof(global_buf));
    charcount=0;
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
       char c = client.read();
       //read char by char HTTP request
        global_buf[charcount]=c;
        if (charcount<sizeof(global_buf)-1) charcount++;
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          dashboardPage(client);
          break;
        }
        if (c == '\n') {
          
          if(strstr(global_buf,"GET /wecker_set.php") > 0 ){
            sscanf(global_buf, "GET /wecker_set.php?wecker_hour=%i&wecker_min=%i HTTP/1.1", &wecker_hour, &wecker_min);        
          }

          if (strstr(global_buf,"GET /wecker_aus") > 0){
            wecker_hour = 0;
            wecker_min = 0;
          }
                    
          if (strstr(global_buf,"GET /power_off") > 0){
            power_status_new = 0;
          }
          else if (strstr(global_buf,"GET /power_on") > 0){
            power_status_new = 1;
          }
          // you're starting a new line
          currentLineIsBlank = true;
          memset(global_buf,0,sizeof(global_buf));
          charcount=0;          
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
  }



  
}

time_t getNTPTime()
{
  time_t epoch = 0;
  int retries = 0;

  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  int replyLen;
  do {
    delay(1000);
    replyLen = Udp.parsePacket();
  } while ((retries++ < 15) && (!replyLen));

  if (!replyLen) {  //run out of retries?
    setSyncInterval(30);   //try again in a few seconds
  }
  else
  {
    setSyncInterval(NTP_SYNC_INTERVAL);
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    epoch = secsSince1900 - seventyYears;
  }
  return epoch;
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress & address)
{
  //Serial.println("1");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  //Serial.println("2");
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  //Serial.println("3");

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println("4");
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  //Serial.println("5");
  Udp.endPacket();
  //Serial.println("6");
}

byte wifiConnect() {
  int i=0;

  WiFi.config(own_ip, dns_ip, gateway_ip, subnet_ip);
  
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) 
  {
    return -1;
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED && i<3) 
  {
    status = WiFi.begin(ssid, pass);
  
    // wait 10 seconds for connection:
    delay(10000);
    i++;
  }

  if(status != WL_CONNECTED)
  {
    return -1;
  }
  return 0;
}

int ntpQuery() {
  char versuche = 3, i=0;
  
  Udp.begin(localPort);

  //Set up NTP Time callback
  setSyncInterval(NTP_SYNC_INTERVAL);   //sync to NTP this often
  setSyncProvider(getNTPTime);

  while (timeSet != timeStatus())                                             //wait for initial time sync to succeed.  getNTPTime will reset the sync interval to 15 seconds if NTP fails
  {
    if(i == versuche)
    {
      return -1;      
    }
    time_t timenow = now();
    delay(1000);
    i++;    
  }

  return 0;
}

void dot(bool stat)
{
  if(stat)
  {
      matrix2.drawPixel(7, 1, LED_ON);  
      matrix2.drawPixel(7, 2, LED_ON);
      matrix2.drawPixel(7, 4, LED_ON);
      matrix2.drawPixel(7, 5, LED_ON);
      matrix2.writeDisplay();
    
      matrix3.drawPixel(0, 1, LED_ON);  
      matrix3.drawPixel(0, 2, LED_ON);
      matrix3.drawPixel(0, 4, LED_ON);
      matrix3.drawPixel(0, 5, LED_ON);
      matrix3.writeDisplay();     
  }
  else
  {
      matrix2.drawPixel(7, 1, LED_OFF);  
      matrix2.drawPixel(7, 2, LED_OFF);
      matrix2.drawPixel(7, 4, LED_OFF);
      matrix2.drawPixel(7, 5, LED_OFF);
      matrix2.writeDisplay();
    
      matrix3.drawPixel(0, 1, LED_OFF);  
      matrix3.drawPixel(0, 2, LED_OFF);
      matrix3.drawPixel(0, 4, LED_OFF);
      matrix3.drawPixel(0, 5, LED_OFF);
      matrix3.writeDisplay();
  }
}

void set_disp(unsigned char disp, int time_value, int temp_value)
{
  if(disp == 1)
  {
    matrix1.clear();
    matrix1.drawChar(3,0,time_value+48, 1, 0, 1);
    matrix1.drawChar(11,0,temp_value+48, 1, 0, 1);
    matrix1.writeDisplay();
  }

  else if(disp == 2)
  {
    matrix2.clear();
    matrix2.drawChar(0,0,time_value+48, 1, 0, 1);
    matrix2.drawChar(8,0,temp_value+48, 1, 0, 1);
    //matrix2.drawPixel(15, 5, LED_ON);  
    matrix2.drawPixel(15, 6, LED_ON);
    matrix2.writeDisplay();
  }

  else if(disp == 3)
  {
    matrix3.clear();
    matrix3.drawChar(3,0,time_value+48, 1, 0, 1);
    matrix3.drawChar(11,0,temp_value+48, 1, 0, 1);
    matrix3.writeDisplay();
  }

  else if(disp == 4)
  {
    matrix4.clear();
    matrix4.drawChar(0,0,time_value+48, 1, 0, 1);
    matrix4.drawRect(8, 0, 2, 2, 1);
    matrix4.drawChar(11,0,'C', 1, 0, 1);
    matrix4.writeDisplay();
  }
}

float temp_read()
{
  tempsensor.shutdown_wake(0);   // Don't remove this line! required before reading temp

  // Read and print out the temperature
  float c = tempsensor.readTempC();

  //tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere

  return c;
}

boolean summertime_RAMsave(int year, byte month, byte day, byte hour, byte tzHours)
// European Daylight Savings Time calculation by "jurs" for German Arduino Forum
// input parameters: "normal time" for year, month, day, hour and tzHours (0=UTC, 1=MEZ)
// return value: returns true during Daylight Saving Time, false otherwise
{
 if (month<3 || month>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
 if (month>3 && month<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
 if (month==3 && (hour + 24 * day)>=(1 + tzHours + 24*(31 - (5 * year /4 + 4) % 7)) || month==10 && (hour + 24 * day)<(1 + tzHours + 24*(31 - (5 * year /4 + 1) % 7)))
   return true;
 else
   return false;
}


void configureSensorTsl(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("13 ms");
  Serial.println("------------------------------------");
}





