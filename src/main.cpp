#include <Arduino.h>
#include "RunningMedian.h"

#include "XT_DAC_Audio.h"

#include "config.h"

#include <EEPROM.h>
#include "eeprom_helper.h"
#include <soc/sens_reg.h>
#include <soc/sens_struct.h>

XT_DAC_Audio_Class DacAudio(25,0);

//Runing Median to smooth out the bad readings, I also added a capacitor (just as recommended from the espressif documentation)
RunningMedian pressureSensor = RunningMedian(96);

#define ADC_SAMPLES_COUNT 1000
int16_t abuf[ADC_SAMPLES_COUNT];
int16_t abufPos = 0;

uint16_t maxPressureValue = 4095;
uint16_t initialPressureValue = 200;


byte amplitude = 255;   // amplitude 0-255  == 0%-100%
byte granularity = 64;   // not sure if we might need more values
int16_t duration = 20;  // duration in ms
int frequency = 5;      // I may have to change this to float

//added by paul, just ideas:
float amplitudeFactor = 1; // exponent for amplitude curve
float granularityFactor = 1; // 
float durationFactor = 1; // (maybe we need a setting where duration == wavelength or duration == (wavelength/2))

float amplitudeChaosLevel = 0; //factor for adding chaos
float granularityChaosLevel = 1; // 
float durationChaosLevel = 1; //

//for having consistant random values
//source: https://xkcd.com/221/
//multiply these by the chaos factor
float randomValues[] = {-0.9696f, -0.7383f, -0.1374f, -0.0850f, -0.2379f, 0.3011f, -0.5341f, -0.1069f, 0.3108f, 0.0580f, 0.9292f, -0.4195f, -0.2400f, 0.0213f, -0.8425f, -0.6613f,
 -0.7800f, -0.8816f, 0.4157f, -0.0363f, -0.1545f, 0.5518f, 0.0889f, 0.8727f, 0.9513f, 0.9752f, 0.7474f, 0.2842f, 0.5548f, 0.1845f, 0.7581f, -0.0808f, -0.6798f, -0.2080f, -0.5668f,
 -0.8043f, 0.2940f, -0.8670f, 0.7110f, -0.4398f, -0.2438f, -0.8420f, -0.1654f, 0.0724f, 0.7223f, 0.4637f, -0.6736f, 0.9125f, 0.4487f, -0.7509, -0.6757f, -0.1911f, 0.8103f, -0.9485f,
  0.5773f, 0.8865f, 0.8458f, 0.5566f, -0.3487f, 0.3852f, 0.6393f, -0.2699f, 0.9548f, -0.5603f, -0.3076f, 0.5236f, 0.5266f, 0.6133f, -0.1148f,  0.7357f, -0.5228f, 0.4441f, 0.7740f,
 -0.3753f, -0.8164f, 0.7215f, 0.1414f, -0.6175f, -0.1875f, -0.1939f, 0.8325f, 0.5320f, -0.4221f, -0.8863f, -0.3980f, 0.1201f, -0.2651f,   -0.5098f, 0.3361f, -0.2304f, -0.7100f,
 -0.3909f, 0.4758f, 0.0272f, -0.4729f, 0.5858f, 0.7311f, -0.4239f, 0.4028f, 0.4614, -0.9315f, 0.0262f, -0.0108f, -0.6078f, 0.4519f, 0.8679f, 0.6265f, -0.1315f, 0.6465f, 
 0.0805f, 0.5181f, -0.0144f, 0.3810f, -0.5035f, 0.4240f, -0.4698f, 0.5364f, -0.6245f, -0.7053f, 0.9085f, 0.1422f, 0.6955f, -0.3405f, 0.2238f, -0.7505f, 0.2906f, 0.4683f, 
 -0.8711f, -0.9464f, 0.9163f, -0.0080f, 0.9776f, 0.2133f, -0.6037f, -0.5884f, -0.2536f, -0.9782f, 0.9295f, -0.3542f, 0.2352f, 0.4909f, 0.1730f, 0.0054f, -0.7615f, 0.0952f, 
 -0.8319f, -0.1384f, -0.3837f, -0.9065f,  0.2485f}

char lastCommand[128];

bool debug_mode = false;

uint16_t update_spacing = 20; //in ms rate at which serial should send stuff

XT_Instrument_Class VibeOutput(INSTRUMENT_NONE, 127);

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
    delay(20); // <----- DELAY?
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
      byte _granularity =  (byte) get_value_from_string(s, ',', 1).toInt();      
      int16_t _frequency = (int16_t) get_value_from_string(s, ',', 2).toInt();
      byte _amplitude =    (byte) get_value_from_string(s, ',', 3).toInt();      
      int16_t _duration =  (int16_t) get_value_from_string(s, ',', 4).toInt();

      // ---------------> write to the global variables here <--------------------
      
      VibeOutput.SetFrequency(_frequency);
      VibeOutput.SetDuration(_duration);
      VibeOutput.Volume = _amplitude;
      //DacAudio.Play(&VibeOutput);

      granularity = (255/_granularity);

      Serial.println("Received: " + String(_granularity) + " " + String(_frequency) + " " + String(_amplitude)) + " " + String(_duration);              
    }
    break;
    case 1: //calibrate Max Pressure
    {
      get_pressure_sensor_value(&maxPressureValue, 64);

      //Serial.println("Max Value calibrated! "+String(maxPressureValue));
      //add 10% to the max value as compensation
      maxPressureValue -= initialPressureValue;
      maxPressureValue = (int)(maxPressureValue * 1.1f);
      
    }
    break;
    case 2: //calibrate Min Pressure + Store in Flash
    {
      get_pressure_sensor_value(&initialPressureValue, 256);

      //add 10% to the min value as compensation
      initialPressureValue = (int)(initialPressureValue * 1.15f);
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
    case 5: //Play Wave
    {
      strcpy(lastCommand, s.c_str());        
      int16_t _frequency = (int16_t) get_value_from_string(s, ',', 1).toInt();
      byte _amplitude =    (byte) get_value_from_string(s, ',', 2).toInt();      
      int16_t _duration =  (int16_t) get_value_from_string(s, ',', 3).toInt();
      
      VibeOutput.SetFrequency(_frequency);
      VibeOutput.SetDuration(_duration);
      VibeOutput.Volume = _amplitude;
      DacAudio.Play(&VibeOutput);

      Serial.println("Received: " + String(_frequency) + " " + String(_amplitude)) + " " + String(_duration);
    }
    break;
    case 6: //curves & chaos
    {
      strcpy(lastCommand, s.c_str());  
      // ---------------> update to functions <--------------------
      // ---------------> update to chaos <--------------------
      // format: Type, min, slope, chaos,
      // except granularity: GrainCount, slope, chaos
      byte _granularity =  (byte) get_value_from_string(s, ',', 1).toInt();      
      int16_t _frequency = (int16_t) get_value_from_string(s, ',', 2).toInt();
      byte _amplitude =    (byte) get_value_from_string(s, ',', 3).toInt();      
      int16_t _duration =  (int16_t) get_value_from_string(s, ',', 4).toInt();

      // ---------------> write to the global variables here <--------------------
      
      VibeOutput.SetFrequency(_frequency);
      VibeOutput.SetDuration(_duration);
      VibeOutput.Volume = _amplitude;
      //DacAudio.Play(&VibeOutput);

      granularity = (255/_granularity);

      Serial.println("Received: " + String(_granularity) + " " + String(_frequency) + " " + String(_amplitude)) + " " + String(_duration);              
    }
    break;
    }
  }  
}

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

  //Load the initial compensation value from flash
  initialPressureValue = EepromReadInt(INITIALVALUEADDRESS);    // <---- check this!
  Serial.println("Initial Pressure Value Loaded " + String(initialPressureValue));  
}

// this is the messy part where I try to process the pressure readings to something that is universal for everyone person
// ToDo: FIX ME!
inline byte get_mapped_pressure_value(){
  //get the current average "smoothed" value from the RunningMedian, we could also .getMedian to get the Median.    
  //read raw pressure value
  uint16_t pressureValue = analogRead(34);

  //check if the value lies below the "idle" sensormeasurement if so set it to 0 if not reduce it's value by the "idle" value
  if (pressureValue < initialPressureValue)
    pressureValue = 0;
  else
    pressureValue -= initialPressureValue;

  //add this value to our RunningMedian of 96 values
  pressureSensor.add(pressureValue);

  int averagePressureValue = pressureSensor.getMedian(); // <----------ToDo:  replace with lowpass filter
  //now remove any values that exceed our upper bound
  if (averagePressureValue > maxPressureValue)
  {
    averagePressureValue = maxPressureValue;
  }

  //now interpolate the values to a range that goes from 0-255 this "unifies" the values so that every person get's the same (nothing)0-255(person's max weight) readings
  return map(averagePressureValue, 0, maxPressureValue, 0, 255);
}



uint16_t lastStep = 0;
//Handles Granularity
//todo: this function should check if a grain shoudl be played, and then find the right parameters and play them
inline void play_at_step(byte mappedPressure){
    byte currentStep = (mappedPressure / granularity);

    // ------------------->> change to check functions here <<---------------------
    
    if(true){    // if in new interval, play sound  
     Serial.println("FIX THIS");
     // update the vibe settings 

     // VibeOutput.SetFrequency(_frequency); // <---- find right parameters
     // VibeOutput.SetDuration(_duration);
     // VibeOutput.Volume = _amplitude;

      DacAudio.StopAllSounds();
      DacAudio.Play(&VibeOutput);
    }    
}

unsigned long lastTimestamp = 0;
byte last_mapped_pressure_value = 0;
void loop()
{
  DacAudio.FillBuffer();    

  //See if Serial Input is available and process the command
  if (Serial.available())
    process_commands(Serial.readStringUntil('\n'), false);


  //get our pressure universal pressure readings 0-255 wheres as 255 equals the max body weight  
  byte mapped_pressure_value = get_mapped_pressure_value();

  //display the current pressure value
  ledcWrite(BLUELED, ledValue[mapped_pressure_value]);

  play_at_step(mapped_pressure_value);

  //Send feedback back to Shoe not constanly only at fixed intervals note
  if (debug_mode) // <--- ToDo (later) maybe add a streaming mode for recording footstep
  {
    unsigned long currentTime = millis();
    if (currentTime - lastTimestamp >= update_spacing)
    {
      lastTimestamp = currentTime;

      if(true) //if (mapped_pressure_value != last_mapped_pressure_value) //just send stuff when something changes
      {
        last_mapped_pressure_value = mapped_pressure_value;
        String debug_message = "Pressure "+ String(last_mapped_pressure_value)+" Raw "+analogRead(34);

        Serial.println(debug_message);
      }
    }
          
  }    
}