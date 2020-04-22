#include <Arduino.h>
#include "RunningMedian.h"

#include "XT_DAC_Audio.h"

#include "config.h"

#include <EEPROM.h>
#include "eeprom_helper.h"
#include <soc/sens_reg.h>
#include <soc/sens_struct.h>

#include <regex>

XT_DAC_Audio_Class DacAudio(25,0);

//Runing Median to smooth out the bad readings, I also added a capacitor (just as recommended from the espressif documentation)
RunningMedian pressureSensor = RunningMedian(7);


//filter stuff
float previousValue = 0; //variable for temporarily storing previous value
double linearizedPressureValue = 0; //made a global variable, so I can use it for debugging
float apply_lowpass_filter(int _inputValue, float k){ 
  //float k = 0.35; //filter strength 0.00001 == very strong 0.99999 == very week
  float rawInput = (float)_inputValue;
  float filteredOutput = (rawInput*k)+(previousValue*(1-k));
  previousValue = filteredOutput;
  return filteredOutput;
}


#define ADC_SAMPLES_COUNT 1000 //todo: check if this is good

int16_t abuf[ADC_SAMPLES_COUNT];
int16_t abufPos = 0;

//uint16_t maxPressureValue = 4095;
uint16_t maxPressureValue = 2000;
uint16_t initialPressureValue = 100; //todo: check this.
//uint16_t initialPressureValue = 200; //todo: check this.

// byte amplitude = 255;   // amplitude 0-255  == 0%-100%
// byte granularity = 64;   // not sure if we might need more values
// int frequency = 5;      // I may have to change this to float

// duration is now automatically computed by default; if forced, the duration value is used
boolean forceDuration = false;
int16_t duration = 20;  // duration in ms

//for having consistant random values
//source: https://xkcd.com/221/
//multiply these by the chaos factor
byte rvNb = 255; // size of the randomValues array
float randomValues[] = { -0.8909f,0.2538f,0.4904f,0.75f,-0.2833f,0.3298f,0.4539f,-0.8135f,-0.1595f,0.5388f,-0.8456f,
0.6324f,0.2644f,-0.2751f,0.2321f,0.5411f,-0.069f,-0.8031f,0.7303f,-0.2281f,0.531f,-0.9253f,-0.1213f,-0.5805f,-0.2507f,
-0.9697f,-0.2586f,-0.6236f,-0.1116f,-0.3419f,0.8754f,-0.377f,0.3225f,0.4548f,0.2876f,-0.8417f,-0.4124f,-0.7076f,0.399f,
-0.9862f,-0.764f,-0.3147f,-0.8101f,0.566f,0.5388f,-0.8506f,0.6395f,-0.8267f,0.1767f,0.1509f,-0.1278f,-0.7367f,0.9241f,
0.4417f,-0.785f,0.5816f,-0.5467f,-0.3627f,0.7075f,0.6421f,-0.588f,0.1157f,0.1416f,-0.1847f,0.6607f,-0.6565f,0.402f,0.9188f,
0.414f,0.6137f,0.8814f,-0.449f,-0.8157f,-0.2447f,0.4096f,0.0937f,-0.3459f,-0.6725f,0.268f,-0.7573f,-0.949f,-0.5676f,0.3419f,
0.6405f,0.3873f,-0.5851f,-0.6635f,0.1151f,-0.8192f,-0.497f,-0.1897f,0.3672f,0.8501f,-0.7185f,-0.7521f,-0.5164f,0.6666f,
-0.5112f,-0.6369f,-0.4217f,0.6005f,0.17f,-0.1816f,-0.4028f,-0.7997f,0.9511f,-0.8107f,0.3134f,0.969f,0.4102f,0.6205f,0.324f,
-0.9549f,0.0058f,0.3563f,-0.1075f,-0.2954f,0.4392f,-0.2667f,-0.4031f,-0.2123f,0.1851f,-0.0034f,0.0255f,0.8106f,-0.3232f,
0.3028f,0.0323f,-0.1564f,0.5743f,0.8884f,-0.515f,-0.1989f,-0.3077f,0.3381f,0.5863f,-0.4282f,-0.0748f,-0.7515f,0.8101f,
-0.3511f,-0.709f,0.4371f,-0.8021f,0.4543f,-0.9822f,0.319f,-0.5904f,0.6233f,0.1182f,0.7887f,-0.2887f,-0.6145f,0.489f,-0.2738f,
-0.4012f,-0.4878f,-0.4563f,-0.781f,0.4997f,-0.3466f,-0.3978f,0.5037f,0.2296f,0.2989f,0.409f,0.6721f,0.8646f,-0.5008f,0.1501f,
0.2457f,0.9214f,0.3602f,-0.3235f,-0.4684f,0.5841f,-0.9264f,0.1935f,-0.8311f,-0.8753f,-0.8925f,-0.5282f,-0.3907f,0.8803f,0.4182f,
0.6001f,-0.2588f,-0.1521f,-0.4074f,0.8464f,-0.9337f,0.1638f,0.1716f,0.8362f,-0.0605f,-0.2011f,0.0135f,-0.0881f,0.4376f,-0.4115f,
-0.1019f,-0.2473f,0.6075f,0.6295f,0.2242f,-0.8903f,-0.4468f,0.1244f,0.7364f,-0.2265f,0.1948f,-0.5735f,-0.084f,-0.5604f,0.3941f,
0.173f,0.5524f,0.0142f,0.4948f,-0.0987f,-0.6867f,0.3425f,-0.5745f,0.5013f,-0.5693f,-0.1514f,0.4609f,-0.357f,-0.3126f,0.899f,
0.6771f,0.7978f,0.4066f,0.053f,-0.943f,-0.2009f,0.0042f,0.1819f,0.8908f,-0.7376f,-0.0111f,0.8684f,-0.6467f,-0.29f,-0.6037f,
0.1171f,0.1398f,-0.1724f,-0.0264f,0.6255f,-0.1737f,-0.2231f,-0.164f,0.9396f,0.6087f };

// NEED ALSO NEGATIVE NUMBERS

// to move somewhere else
std::regex regex_number("[0-9\\.]+");



/*
  Variables used by all parameters
*/
byte grainsNb;   // number of grains
byte minBoundIndex, maxBoundIndex; // indices of the bounds (should be between 0-#grains)
byte lastGrainIndex; // last grain played -- needed to avoir bouncing

void reset_grain_number(byte gnb) {
  grainsNb = gnb;
  minBoundIndex = 0;
  maxBoundIndex = 0;
  lastGrainIndex = 0;
}

/* #region Parameters */ 
int *granularity, *frequency, *amplitude;

// reset all parameters related to granularity based on the number of grains and the exponent
// this function considers that the force input is by default at 0
void update_parameter_values(std::string data, int *param, String name)
{

  if(param) { delete[] param; }
  param = new int[grainsNb];

  byte i = 0;
  std::smatch sm; // results of the regular expression search
  String all = name+"\n";
  while (i < grainsNb && regex_search(data, sm, regex_number)) {
    String match = String(sm[0].str().c_str());
    all += match + " ";
		granularity[i] = (byte) match.toInt(); // super dupper ugly
		data = sm.suffix().str();
	}
  Serial.println(all);
}

// we want durations above 4ms 
byte compute_duration(int frequency) {
  double duration = 1000.0 / frequency;
  while(duration < 4.0) { duration *= 2.0; }
  return (byte) duration;
}
/* #endregion */


char lastCommand[128];

bool useGrains = true; // avoid conflicts when playing waves (command ID: 5)

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
    delay(1); // <----- DELAY? (was 20)
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
    case 0: // set parameters; duration is now automatically computed
    {
      // DEPRECATED

      // useGrains = true;

      // strcpy(lastCommand, s.c_str());  
      // byte _granularity =  (byte) get_value_from_string(s, ',', 1).toInt();      
      // int16_t _frequency = (int16_t) get_value_from_string(s, ',', 2).toInt();
      // byte _amplitude =    (byte) get_value_from_string(s, ',', 3).toInt();
      // // int16_t _duration =  (int16_t) get_value_from_string(s, ',', 4).toInt();
      // String sdur = get_value_from_string(s, ',', 4);
      // if(sdur.equals("")) {
      //   forceDuration = true;
      // } else {
      //   forceDuration = false;
      //   duration = sdur.toInt();
      // }

      // reset_granularity_parameters(_granularity, 1.f, 0, false); // important to set granularity first, the other dimensions depend on it
      // reset_frequency_parameters(0, _frequency, 0.f, 0, false); // static frequency
      // reset_amplitude_parameters(0, _amplitude, 0.f, 0, false); // static amplitude

      // Serial.println("Received: " + String(_granularity) + " " + String(_frequency) + " " + String(_amplitude) + " " + sdur);              
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
      useGrains = false;

      strcpy(lastCommand, s.c_str());        
      int16_t _frequency = (int16_t) get_value_from_string(s, ',', 1).toInt();
      byte _amplitude =    (byte) get_value_from_string(s, ',', 2).toInt();      
      int16_t _duration =  (int16_t) get_value_from_string(s, ',', 3).toInt();
      VibeOutput.SetWaveForm(WAVE_SQUARE);
      VibeOutput.SetFrequency(_frequency);
      VibeOutput.SetDuration(_duration);
      VibeOutput.Volume = (byte) ((_amplitude/100.0)*127.0); // amplitude should be between 0-30%
      DacAudio.Play(&VibeOutput);

      Serial.println("Received: " + String(_frequency) + " " + String(_amplitude)) + " " + String(_duration);
    }
    break;
    case 6: // granularity
    {
      useGrains = true;
      
      strcpy(lastCommand, s.c_str());  

      // parameters: #grains, exponent, chaos scalar
      byte gnb = (byte) get_value_from_string(s, ',', 1).toInt();      
      
      reset_grain_number(gnb);

      std::string data = s.c_str();
      update_parameter_values(data.substr(data.find(',', 2)+1), granularity, "Granularity");
    }
    break;
    case 7: // frequency - 1 or amplitude - 2
    {
      useGrains = true;

      strcpy(lastCommand, s.c_str());

      // parameters: min value, max value, exponent, chaos scalar, symmetry
      byte parameter = (byte) get_value_from_string(s, ',', 1).toInt();
     
      std::string data = s.c_str();
      data = data.substr(data.find(',', 2)+1);
      if(parameter == 1) {
        update_parameter_values(data.substr(data.find(',', 2)+1), frequency, "Frequency");
      } else if(parameter == 2) {
        update_parameter_values(data.substr(data.find(',', 2)+1), amplitude, "Amplitude");
      }
    }
    break;
    }
  }  
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
  double filteredPressureValue = apply_lowpass_filter(averagePressureValue, 0.03f); //using double for more precision
   linearizedPressureValue = pow((filteredPressureValue/300),3.6); //linearize
  //now remove any values that exceed our upper bound
  if (linearizedPressureValue > maxPressureValue)
  {
    linearizedPressureValue = maxPressureValue;
  }

  //now interpolate the values to a range that goes from 0-255 this "unifies" the values so that every person get's the same (nothing)0-255(person's max weight) readings
  return map(linearizedPressureValue, 0, maxPressureValue, 0, 255);
}




uint16_t lastStep = 0;

//Handles Granularity
//todo: this function should check if a grain shoudl be played, and then find the right parameters and play them

//original function
// inline void play_at_step(byte mappedPressure){
//     byte currentStep = (mappedPressure / granularity);
    
//     if(useGrains) {
//       if(currentStep != lastStep) {      
//         lastStep = currentStep;
//         DacAudio.StopAllSounds();
//         DacAudio.Play(&VibeOutput);
//       }   
//     } 
// }


inline void play_at_step(byte mappedPressure){
    // byte currentStep = (mappedPressure / granularity);

    // ------------------->> change to check functions here (consider possible Chaos) <<---------------------
    boolean playGrain = false;  

    // if one of the parameters is not set, we don't play any grain
    if(!granularity || !frequency || !amplitude) {
      Serial.println("Device not configured correctly!");
      return;
    }

    if(mappedPressure < granularity[minBoundIndex]) {
      if(lastGrainIndex != minBoundIndex) {
        playGrain = true;
        lastGrainIndex = minBoundIndex;
      }

      if(minBoundIndex > 0) { maxBoundIndex = minBoundIndex--; }
    }
    else if (granularity[maxBoundIndex] <= mappedPressure) {
      if(lastGrainIndex != maxBoundIndex) {
        playGrain = true;
        lastGrainIndex = maxBoundIndex;
      }

      if(maxBoundIndex < grainsNb) { minBoundIndex = maxBoundIndex++; }
    }

    // Serial.println("MinBound " + String(minBoundValue) + ", MinIndex " + String(minBoundIndex) + ", MaxBound " + String(maxBoundValue) + ", MaxIndex " + String(maxBoundIndex));

    if(playGrain) {    // if in new interval, play sound  
    //  Serial.println("FIX THIS");
     // update the vibe settings 

     // VibeOutput.SetFrequency(_frequency+frequencyChaosLevel*chaosSeed); // <---- find right parameters
     // VibeOutput.SetDuration(_duration);
     // VibeOutput.Volume = _amplitude+amplitudeChaosLevel*chaosSeed;
  
    
      VibeOutput.SetFrequency(frequency[lastGrainIndex]);
      if(!forceDuration) { VibeOutput.SetDuration(compute_duration(frequency[lastGrainIndex])); }
      else { VibeOutput.SetDuration(duration); }
      VibeOutput.Volume = (amplitude[lastGrainIndex]/100.0)*127.0;

      DacAudio.StopAllSounds();
      DacAudio.Play(&VibeOutput);
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
  // Serial.println("Initial Pressure Value Loaded " + String(initialPressureValue));  

  //FIXME setting parameters here create an error later when pressing the shoe
  // initialize all parameters to default values
  // reset_granularity_parameters(20, 1, 0);
  // reset_frequency_parameters(0, 350, 0, 0); // static frequency of 350
  // reset_amplitude_parameters(0, 15, 0, 0); // static amplitude of 15

  // // need to set the right interval for granularity in case the initial pressure value is not 0
  // byte pressure = get_mapped_pressure_value();
  // while(maxBoundValue < pressure) { 
  //   minBoundIndex = maxBoundIndex;
  //   minBoundValue = maxBoundValue; 
  //   maxBoundValue = compute_grain_level(++maxBoundIndex, true); // in theory, impossible to overshoot so no need to check upper limit
  // }
  // Serial.println(String(pressure) + " " + String(minBoundIndex) + " " + String(maxBoundIndex));
}



unsigned long lastTimestamp = 0;
byte last_mapped_pressure_value = 0;
void loop()
{
  DacAudio.FillBuffer(); 
  int bufferusageTEMP =  DacAudio.BufferUsage();
  //Serial.println(bufferusageTEMP);

  //See if Serial Input is available and process the command
  if (Serial.available())
    process_commands(Serial.readStringUntil('\n'), false);


  //get our pressure universal pressure readings 0-255 wheres as 255 equals the max body weight  
  byte mapped_pressure_value = get_mapped_pressure_value();
    DacAudio.FillBuffer(); 
  //display the current pressure value
  ledcWrite(BLUELED, ledValue[mapped_pressure_value]);

  if(useGrains) { play_at_step(mapped_pressure_value); }

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
        String debug_message = "Pressure "+ String(last_mapped_pressure_value*8) +", " +"Linearized Pressure "+ String(linearizedPressureValue)+", Filtered Pressure "+ String(previousValue)+", Raw "+ String(analogRead(34)-initialPressureValue);

        Serial.println(debug_message);
      }
    }
          
  }    
}

