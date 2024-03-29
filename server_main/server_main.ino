/*
 * This is the FW for the freezer alaram.  Internal freezer temperature is monitored
 * and if the temperature falls below the trip point, the alarm will alert the user 
 * via email.  The alarm will also serve up a web page allowing real time statistics 
 * to be viewed, and parameters to be adjusted. 
 * 
 * TODO: The handling of the "Test Email" feature is 
 * pretty crude, and should possibly be improved upon 
 * 
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#include <SPI.h>


/**
 * @brief SW Version Info
 * 
 */
#define SW_VERSION              2.1
 
/**
 * Uncomment the following 
 * if you wish to have logging 
 * data printed to the screen
 */
#define ENABLE_LOGGING            true

/**
 * WiFi Parameters
 */
#define WIFI_CONNECT_TIMEOUT_S    10
WiFiClient client;

/**
 * Email related parameters
 */
String    subject       = "";
String    email_body    = "";
char email_server[] = "mail.smtp2go.com";     // smtp2go email server

/* BASE 64 Encoded SMTP2GO User Account */
char buf_hyg_smtp2go_account[] = "Y2xpbnRvbi5ndWVudGhlckBnbWFpbC5jb20=";

/* BASE 64 Encoded SMTP2GO User Password */
char buf_hyg_smtp2go_password[] = "TUolZGZoKiY0NQ==";

/* Recipient's email address  */
char buf_recipient_email_addr[] = "clinton.guenther@gmail.com";

/* Source email address */
char buf_sender_email_addr[] = "clinton.debug@gmail.com";



#define TEMP_BUF_SIZE             64    // Size of the temporary buffer
char buf_temp[TEMP_BUF_SIZE];           // Temporary buffer that can be used for building strings

// Fill in your WiFi router SSID and pass mword
const char*     ssid = "CJG_GbE_2G4";           //My Home Router SSID
const char*     password = "GlockHK23";         //My Home Router Password
float           trip_pt_flt = 10;               //A freezer temperature equal to or above this is considered critical
float           bat_voltage = 3.86;             //Container for value of backup battery's voltage
String          html_string = "";

// GPIO PIN SETUP
const int GRN_LED  = 5;     //GPIO #5 is the led pin for the 8266 thing and the green LED of the freezer alarm
const int RED_LED  = 0;     //GPIO #0 is the RED LED of the Freezer Alarm.  
int       PWR_GOOD = 4;     //GPIO #4 'looks' at the power input status.  If logic zero, we've lost AC input power
const int SPI_CS  = 16;     //Chip select pin to the temperature measurement IC

//TIMING PARAMETERS
long            tmr1_write_val=151515;   // Empirically derived and verified on scope.  The one second flash rate was verified, anyway.    
unsigned int    ms_ticks_50;
unsigned int    ms_ticks_500;
unsigned int    ms_ticks_1000;

bool            Timer50msFlag   = false;
bool            Timer500msFlag  = false;
bool            Timer1000msFlag = false;
bool            timer_pause_1   = false;      //This will prevent a loop from running multiple times during its current second
bool            timer_pause_2   = false;     

long            seconds_counter = 0;        //32bit value 4.264....e9

//TEMPERATURE AND POWER
float           temp_buffer[8];                       // Keep FIFO buffer for temperature averaging
int             circle_buf_ptr = 0;                   // Keep track of which sample to 
float           global_temp_flt = -0.0;               //Value updated during analog read of temperature

bool            critical_condition_detected = false;      // Send immediate warning if freezer temp is critical, but then send periodic after that
bool            initial_warning_sent = false;             //This flag is set only inside the loop whose job is to send warning emails
bool            temp_is_critical = false;                 // This flag is set when the measured temperature is warmer than the trip point
bool            pwr_is_good = true;                       // This flag is cleared if we loose AC input power
bool            reboot_POST = false;                      // We want to send an email at restart.  

ESP8266WebServer server(80);    //Initialize the server library


void ICACHE_RAM_ATTR onTimerISR(){
  timer1_write(tmr1_write_val);

  Timer50msFlag = true;

  if(ms_ticks_50 == 10) {
    ms_ticks_50 = 0;               //Reset centi-seconds
    Timer500msFlag = true;
  
    if(ms_ticks_500 == 2) {         //See if we need to roll over
      ms_ticks_500 = 0;
      Timer1000msFlag = true;  
    }

    else {
      ms_ticks_500++;              //Increase 500ms timer
    }

  }
  
  else {
      ms_ticks_50++;
  }

}

void handleRoot()
{
  String argument_message = "";

  #if defined (ENABLE_LOGGING)
    for (int i = 0; i < server.args(); i++) {
      argument_message += "Arg no" + (String)i + " –> ";
      argument_message += server.argName(i) + ": ";
      argument_message += server.arg(i) + "\n";
    } 
      Serial.print("***DEBUG "); Serial.println(argument_message);
  #endif

  if (server.hasArg("email_test")) {

    #if defined (ENABLE_LOGGING)
      Serial.println("****Sending test email "); 
    #endif
    
    #if defined (ENABLE_LOGGING)
      Serial.println("****DEBUG Refreshing browser.");
    #endif
    RefreshPage();            
    server.send(200, "text/html", html_string);
    
    email_body =  "Just a test email.";
    send_email_message();
  }

  else if (server.hasArg("trip_pt")) {
    handleSubmit();
  }
  
  else {
    RefreshPage();            
    server.send(200, "text/html", html_string);
  }
}

void returnFail(String msg)
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(500, "text/plain", msg + "\r\n");
}


void handleSubmit()
{
  String LEDvalue;
  String trip_pt_str;
  String email_freq_str;

  if (!server.hasArg("trip_pt")){
    Serial.println("Bad Args.");
    return returnFail("BAD ARGS");
  } 

  trip_pt_str = server.arg("trip_pt");            //Returned as a string value

  trip_pt_flt = trip_pt_str.toFloat();


  //Action taken, now refresh browser
  #if defined (ENABLE_LOGGING)
    Serial.print("****DEBUG Trip Point Updated Value: "); Serial.println(trip_pt_flt);
    Serial.println("****DEBUG Refreshing browser.");
  #endif
  
  RefreshPage();
  server.send(200, "text/html", html_string);

  returnOK();

}

void returnOK()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "OK\r\n");
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void RefreshPage (void) {
  html_string = "<!DOCTYPE HTML>";
  html_string += "<html>";
  html_string += "<head>";
  html_string += "<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">";
  html_string += "<title>CJG Freezer Monitor</title>";
  html_string += "<style>";
  html_string += "\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\"";
  html_string += "</style>";
  html_string += "</head>";
  html_string += "<body>";
  html_string += "<h1>CJG Freezer Monitor</h1>";
  html_string += "<h3>SW Version " + String(SW_VERSION) + " </h3>";   
  html_string += "<FORM action=\"/\" method=\"post\">";
  html_string += "<P>";
  html_string += "Current Temperature: " + String(global_temp_flt) + " &#176F";
  html_string += "<br>";
  html_string += "Critical Temp Setting: " + String(trip_pt_flt) + " &#176F";
  html_string += "<br><br>";
  
  html_string += "Update Trip Point: <INPUT type=\"text\" name=\"trip_pt\" size=\"5\" value=\"" + (String)trip_pt_flt + "\"<br>";
  html_string += "<br><br>";
  html_string += "<input type=\"submit\" name=\"action\" value=\"Update\">";
  
  /* Test Email Button */
  html_string += "<br><br>";
  html_string += "<button name = \"email_test\" value = \"email_test\" type = \"submit\">Email Test</button>";
  
  html_string += "</P>";
  html_string += "</FORM>";
  html_string += "</body>";
  html_string += "</html>";

}

int TwosConvert(int x) {
  x ^= 0xFFFF;
  x += 0x0001;
  x &= 0x3FFF;    //Only care about the 14 temperature bits
  return(x);
}

float get_temperature(void) {

  int i                   = 0;  
  long raw_therm_reading  = 0;
  float temp_sum          = 0.0;
  int signed_temp_data    = 0;
  float temp_value        = -0.0;
  
  digitalWrite(SPI_CS,0);     //Drop CS to enable the transaction
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));  // Defaults
  delay(5);                   //Delay in ms

  raw_therm_reading = SPI.transfer(0x00);                               //Reads bits 31:24
  raw_therm_reading = (raw_therm_reading << 8) | SPI.transfer(0x00);    //Read bits 23:16
  raw_therm_reading = (raw_therm_reading << 8) | SPI.transfer(0x00);    //Read bits 15:8
  raw_therm_reading = (raw_therm_reading << 8) | SPI.transfer(0x00);    //Read bits 7:0
  
  SPI.endTransaction();
  delay(5);                   //Delay in ms
  digitalWrite(SPI_CS,1);     //CS back high now that SPI transaction is over

  signed_temp_data = (int)((raw_therm_reading >> 18) & 0x3FFF);     //Going to pack the result as an integer 

  if(signed_temp_data & (0x2000)) {
    signed_temp_data = TwosConvert(signed_temp_data);
    temp_value = float(signed_temp_data/-4.0);
  }
  else {
    temp_value = float(signed_temp_data / 4.0);
  }
  
  temp_value = float((temp_value * 1.8) + 32);

  temp_buffer[circle_buf_ptr] = temp_value;
  
  (circle_buf_ptr >= 7) ? (circle_buf_ptr = 0):(circle_buf_ptr++);

  temp_sum = 0.0;   // Reset this value in preparation to calculate the average
  
  for(i=0; i<8; i++) {
    temp_sum += temp_buffer[i];
  }

  temp_value = float(temp_sum / 8);

  return(temp_value);

}

void setup(void) {
  
  pinMode(GRN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();

  RefreshPage();

  //Initialize SPI Peripheral
  pinMode(SPI_CS, OUTPUT);      //Chip select line to the temperature sensor must be an output
  pinMode(15,OUTPUT);         //Guarantee the slave works in master mode 
  digitalWrite(SPI_CS,1);     //Make sure the CS line is high
  SPI.begin();

  //Initialize Ticker every 0.05s
  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(tmr1_write_val);        //.05s 

  #if defined(ENABLE_LOGGING) 
    Serial.println("****DEBUG Rebooted");
  #endif

}

void loop(void)
{

  server.handleClient();

  if(Timer50msFlag == true) {
    Timer50msFlag = false;
  }

  if(Timer500msFlag == true) {
    Timer500msFlag = false;
    global_temp_flt = get_temperature();
  }

  if(Timer1000msFlag == true) {
    Timer1000msFlag = false;
    digitalWrite(GRN_LED,!(digitalRead(GRN_LED)));  //Toggle LED Pin
    (seconds_counter == 300000)?(seconds_counter = 0):(seconds_counter++);
    timer_pause_1 = false;
    timer_pause_2 = false;
  }

  if(seconds_counter % 5 == 0 & timer_pause_2 == false) {                  // Check temperature and power status
    timer_pause_2 = true;
    
    
    /**
     * Check to see if the temperature value is above the set threshold.  
     * Do not care if freezer temp is negative since trip point
     *  will always be zero or positive 
     */
    ((global_temp_flt > trip_pt_flt)) ? (temp_is_critical = true):(temp_is_critical = false);
    
    #if defined(ENABLE_LOGGING) 
      Serial.print("****DEBUG Temp:"); Serial.println(global_temp_flt);
      if(temp_is_critical)
        Serial.println("****DEBUG Temp critical");
    #endif
      

    /**
     * @brief Check AC Mains Power
     * 
     */
    pwr_is_good = digitalRead(PWR_GOOD);   

    if((temp_is_critical | !pwr_is_good)) {  
      critical_condition_detected = true;
      digitalWrite(RED_LED,1);              //Illuminate red LED to indicate a problem.  
    }

    if((!temp_is_critical & pwr_is_good)) {
      critical_condition_detected = false;
      initial_warning_sent = false;
      digitalWrite(RED_LED,0);              //Illuminate red LED to indicate a problem.  
    }         

  }
  
  if(
    ((seconds_counter % 86400 == 0) & timer_pause_1 == false)                | 
    (critical_condition_detected == true & initial_warning_sent == false)     |
    (temp_is_critical & (seconds_counter % 600 == 0) & timer_pause_1 == false) |
    (!pwr_is_good & (seconds_counter % 630 == 0) & timer_pause_1 == false)     |
    (reboot_POST == false)
    ) {

    timer_pause_1 = true;  
    initial_warning_sent = true;              //Initial warning sent, so set flag to prevent subsequent messages

    #if defined(ENABLE_LOGGING)
      Serial.println("****DEBUG Email sending loop");  
      Serial.println("****DEBUG Seconds Counter: " + String(seconds_counter));  
    #endif

    /**
     * @brief Temp Critical, Send Email
     */
    if(temp_is_critical) {
      email_body =  "Temp critical: " + (String)global_temp_flt + 
                    "\u00B0 F (critical trip point set to: " + (String)trip_pt_flt + "\u00B0 F).";
      
      if(send_email_message()) {
        #if defined(ENABLE_LOGGING)
          Serial.println("Critical temp email successfully sent.");
        #endif
      }
      else {
        #if defined(ENABLE_LOGGING)
          Serial.println("Critical temp email failed.");
        #endif
      }
    }

    // AC mains power issue, so send email
    else if(!pwr_is_good) {
      subject = "AC Mains Power Down";
      email_body =  "AC mains power is down.\r\nCurrent Freezer Temp: " + (String)global_temp_flt + 
                    "\u00B0 F (critical trip point set to: " + (String)trip_pt_flt + "\u00B0 F).";
      
      if(send_email_message()) {
        #if defined(ENABLE_LOGGING)
          Serial.println("AC Mains email successfully sent.");
        #endif

      }
      else {
        #if defined(ENABLE_LOGGING)
          Serial.println("AC Mains email failed.");
        #endif
      }
    }
    
    // Everything normal, so send heartbeat email  
    else {
      email_body =  "Freezer alarm healthy. Current temp: " + (String)global_temp_flt + 
                    "\u00B0 F (critical trip point set to: " + (String)trip_pt_flt + "\u00B0 F).";
      
      if(send_email_message()) {
      #if defined(ENABLE_LOGGING)
        Serial.println("Heartbeat email successfully sent.");
      #endif

      }
      else {
        #if defined(ENABLE_LOGGING)
          Serial.println("Heartbeat email failed.");
        #endif
      }
    }
    
    // Freezer alarm just rebooted
    if(reboot_POST == false) {
      email_body = "Freezer alarm rebooted.";
      
      if(send_email_message()) {
      #if defined(ENABLE_LOGGING)
        Serial.println("Reboot POST email successfully sent.");
      #endif

      }
      else {
        #if defined(ENABLE_LOGGING)
        Serial.println("Reboot POST email failed.");
        #endif
      }
      reboot_POST = true;     // Set flag that indicates we've sent the reboot post.  
    }

  }
}
