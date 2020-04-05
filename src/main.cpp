#include <Arduino.h>
#include "RunningMedian.h"
#include <WiFi.h>
#include "XT_DAC_Audio.h"

#include "privatedata.h"
#include "config.h"



#include <EEPROM.h>
#include "eeprom_helper.h"

XT_DAC_Audio_Class DacAudio(25,0);

//Runing Median to smooth out the bad readings, I also added a capacitor (just as recommended from the espressif documentation)
RunningMedian pressureSensor = RunningMedian(96);


uint16_t maxPressureValue = 4095;
uint16_t initialPressureValue = 200;


byte amplitude = 255;   // amplitude 0-255  == 0%-100%
byte granularity = 4;   // not sure if we might need more values
int16_t duration = 20;  // duration in ms
int frequency = 5;      // I may have to change this to float


bool debug_mode = false;

uint16_t update_spacing = 25;

WiFiServer server(80);
WiFiClient client;


String get_value_from_string(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void get_pressure_sensor_value(uint16_t *value, uid_t measurements)
{
  pressureSensor.clear();
  for (int i = 0; i < measurements; i++)
  {
    pressureSensor.add(analogRead(34));
    delay(20);
  }
  *value = pressureSensor.getAverage();
  Serial.println("Current Pressurelevel " + String(*value));
}

void fade_LED(uint8_t LED)
{
  for (int o = 2; o >= 0; o--)
  {
    for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle++)
    {
      ledcWrite(LED, ledValue[dutyCycle]);
      delay(1);
    }
    delay(10);
    // decrease the LED brightness
    for (int dutyCycle = 255; dutyCycle >= 0; dutyCycle--)
    {
      ledcWrite(LED, ledValue[dutyCycle]);
      delay(1);
    }

    ledcWrite(LED, 0);
    delay(50);
  }
  ledcWrite(LED, 0);
}

XT_Instrument_Class Example(INSTRUMENT_NONE, 127);
void setup()
{
  delay(10);

  //Start Serial Connection
  Serial.begin(115200);

  //Init emulated EEPROM (flash) with 2 bytes for a 16bit value
  EEPROM.begin(2);

  //Setup Pins for LEDs, Pressure Readings
  pinMode(34, INPUT);

  ledcSetup(REDLED, 5000, 8);
  ledcSetup(GREENLED, 5000, 8);
  ledcSetup(BLUELED, 5000, 8);

  ledcAttachPin(BLUELEDPIN, BLUELED);
  ledcAttachPin(REDLEDPIN, REDLED);
  ledcAttachPin(GREENLEDPIN, GREENLED);

  //Setup Wifi Hostname, I'm spamming the setHostname function because it often doesn't want to work for some reason
  WiFi.setHostname(HOSTNAME);
  delay(10);
  WiFi.begin(SSID, PW);
  WiFi.setHostname(HOSTNAME);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(50);
    fade_LED(GREENLED);
  }
  WiFi.setHostname(HOSTNAME);

  Serial.println(WiFi.localIP());

  server.begin();


  //Load the initial compensation value from flash
  initialPressureValue = EepromReadInt(INITIALVALUEADDRESS);    
  Serial.println("Initial Pressure Value Loaded " + String(initialPressureValue));
  
  
  Example.SetFrequency(400);
  Example.SetDuration(1000*30);
  
  //Example.Volume = 127;  
  
  DacAudio.Play(&Example);
}

char lastCommand[128];

/*
Commands have the following structure:
They are always humanreadable (I'm debbugging this stuff with a serial terminal)

Data: command , additionalData , ...
Example: 2,\n
You can add as much Data as you want
*/
inline void process_commands(String s, bool force)
{
  if (s.length() >= 2)
  {
    String type = get_value_from_string(s, ',', 0);
    Serial.println("Command: " + String(type));
    switch (type.toInt())
    {
    case 0: //Set Settings
    {
      strcpy(lastCommand, s.c_str());  
      byte _granularity = (byte) get_value_from_string(s, ',', 1).toInt();      
      byte _amplitude =   (byte) get_value_from_string(s, ',', 3).toInt();
      int16_t _frequency = (int16_t) get_value_from_string(s, ',', 2).toInt();
      int16_t _duration =  (int16_t) get_value_from_string(s, ',', 4).toInt();
      
      Example.SetFrequency(_frequency);
      Example.SetDuration(_duration);
      Example.Volume = _amplitude;
      DacAudio.Play(&Example);

      Serial.println("Received: " + String(_granularity) + " " + String(_frequency) + " " + String(_amplitude)) + " " + String(_duration);              
    }
    break;
    case 1: //calibrate Max Pressure
    {
      get_pressure_sensor_value(&maxPressureValue, 64);

      Serial.println("Max Value calibrated! "+String(maxPressureValue));
      //add 10% to the max value as compensation
      maxPressureValue -= initialPressureValue;
      maxPressureValue = (int)(maxPressureValue * 1.1f);
      
    }
    break;
    case 2: //calibrate Min Pressure + Store in Flash
    {
      get_pressure_sensor_value(&initialPressureValue, 256);

      //add 10% to the min value as compensation
      initialPressureValue = (int)(initialPressureValue * 1.1f);
      Serial.println("Initial Value calibrated! "+String(initialPressureValue));

      /*
      store it in flash to reuse it later, min compensation is only there to compensate the lower bound of what the pressure sensor is reading
      when it's in idle and no pressure is applied to it
      */
      EepromWriteInt(INITIALVALUEADDRESS, initialPressureValue);
    }
    break;
    case 3: //Turn on Feedback
    {
      debug_mode = true;
      update_spacing = (uint16_t)get_value_from_string(s, ',', 1).toInt();
      Serial.println("Feedback ON!  Updaterate: "+String(update_spacing));
    }
    break;
    case 4: //Turn off Feedback
    {
      debug_mode = false;      
      Serial.println("Feedback OFF!");
    }
    break;
    }
  }  
}

// this is the messy part where I try to process the pressure readings to something that is universal for everyone person
byte get_mapped_pressure_value(){
  
  //read raw pressure value  
  uint16_t pressureValue = analogRead(34);

  //check if the value lies below the "idle" sensormeasurement if so set it to 0 if not reduce it's value by the "idle" value
  if (pressureValue < initialPressureValue)
    pressureValue = 0;
  else
    pressureValue -= initialPressureValue;

  //add this value to our RunningMedian of 96 values
  pressureSensor.add(pressureValue);

  //get the current average "smoothed" value from the RunningMedian, we could also .getMedian to get the Median.
  int averagePressureValue = pressureSensor.getMedian();

  //now remove any values that exceed our upper bound
  if (averagePressureValue > maxPressureValue)
  {
    averagePressureValue = maxPressureValue;
  }

  //now interpolate the values to a range that goes from 0-255 this "unifies" the values so that every person get's the same (nothing)0-255(person's max weight) readings
  return map(averagePressureValue, 0, maxPressureValue, 0, 255);
}


unsigned long timestamp = 0;
byte last_mapped_pressure_value = 0;
void loop()
{
  DacAudio.FillBuffer();  

  //See if a client connects or is connected and receive their Data (over Wifi)
  if (client)
  {
    if (client.connected())
    {
      if (client.available())
      {
        process_commands(client.readStringUntil('\n'), false);
      }
    }
    else
    {
      client.stop();
    }
  }
  else
    client = server.available();

  //See if Serial Input is available and process the command
  if (Serial.available())
    process_commands(Serial.readStringUntil('\n'), false);


  //get our pressure universal pressure readings 0-255 wheres as 255 equals the max body weight  
  byte mapped_pressure_value = get_mapped_pressure_value();

  //display the current pressure value
  ledcWrite(BLUELED, ledValue[mapped_pressure_value]);

  //Send feedback back to Shoe
  if (debug_mode)
  {
    unsigned long currentTime = millis();
    if (currentTime - timestamp >= update_spacing)
    {
      timestamp = currentTime;

      if (mapped_pressure_value != last_mapped_pressure_value)
      {
        last_mapped_pressure_value = mapped_pressure_value;
        String debug_message = "DEBUG;"+ String(last_mapped_pressure_value) + ";";

        if (client)
        {
          if (client.connected())
            client.println(debug_message);
          else
            client.stop();
        }

        Serial.println(debug_message);
      }
    }
      
    //Serial.println(RTC_SLOW_MEM[indexAddress] & 0xffff);    
  }    
}