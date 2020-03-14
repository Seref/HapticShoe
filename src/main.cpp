#include <Arduino.h>
#include "XT_DAC_Audio.h"
#include "RunningMedian.h"
#include <WiFi.h>

#include "privatedata.h"
#include "config.h"
#include "soundFiles.h"

#include <ArduinoOTA.h>

#include <EEPROM.h>
#include "eeprom_helper.h"

XT_DAC_Audio_Class DacAudio(25, 2);

RunningMedian pressureSensor = RunningMedian(96);

XT_Wav_Splitable_Class *ContactSound = nullptr;

uint16_t maxPressureValue = 512;
uint16_t initialPressureValue = 200;

int calculatedMaxPressureValue = 255;

byte volume = 127;
byte currentState = UP;
byte materiallayer = 5;
byte soundsSelection = 1;

soundDataFile const *currentFile;

bool debug_mode = false;
bool game_mode = true;

WiFiServer server(80);
WiFiClient client;

uint16_t update_spacing = 25;

bool gives_in = true;
bool reached_peak = false;
int current_layer = 0;

byte direction_tolerance_counter = 0;

byte last_adjusted_pressure = 0;

unsigned long timestamp = 0;

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

inline void setup_OTA_updates()
{
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });

  ArduinoOTA.begin();
}

bool is_static = false;

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(2);

  pinMode(34, INPUT);

  ledcSetup(REDLED, 5000, 8);
  ledcSetup(GREENLED, 5000, 8);
  ledcSetup(BLUELED, 5000, 8);

  ledcAttachPin(BLUELEDPIN, BLUELED);
  ledcAttachPin(REDLEDPIN, REDLED);
  ledcAttachPin(GREENLEDPIN, GREENLED);

  WiFi.setHostname(HOSTNAME);
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

  setup_OTA_updates();

  initialPressureValue = EepromReadInt(INITIALVALUEADDRESS);

  Serial.println("Initial Pressure Value Loaded " + String(initialPressureValue));
  currentFile = soundList[0];

  ContactSound = new XT_Wav_Splitable_Class(currentFile->soundFile, 0);
  ContactSound->RepeatForever = false;

  DacAudio.Play(ContactSound);
  currentState = PAUSE;
}

char lastCommand[128];
inline void process_commands(String s, bool force)
{

  if (s.length() >= 2)
  {
    String type = get_value_from_string(s, ',', 0);
    Serial.println("Command: " + String(type));
    switch (type.toInt())
    {
    case 0: //Set Sound Value
    {

      strcpy(lastCommand, s.c_str());
      //Data: sound || maxPressureValue || materiallayer || volume
      is_static = false;
      byte currentsoundsSelection = (byte)get_value_from_string(s, ',', 1).toInt();
      calculatedMaxPressureValue = get_value_from_string(s, ',', 2).toInt();
      byte tempmateriallayer = (byte)get_value_from_string(s, ',', 3).toInt();
      volume = (byte)get_value_from_string(s, ',', 4).toInt();

      if (currentsoundsSelection != soundsSelection || materiallayer != tempmateriallayer || force)
      {
        materiallayer = tempmateriallayer;
        soundsSelection = currentsoundsSelection;

        Serial.println("Sound: " + String(soundsSelection) + " maxPressure " + String(maxPressureValue) + " materialLayer " + String(materiallayer)) + " volume " + String(volume);

        DacAudio.StopAllSounds();

        if (ContactSound != nullptr)
          delete ContactSound;

        currentFile = soundList[soundsSelection];
        ContactSound = new XT_Wav_Splitable_Class(currentFile->soundFile, materiallayer);

        currentState = UP;
      }
    }
    break;
    case 1: //max Pressure Value
    {
      get_pressure_sensor_value(&maxPressureValue, 64);
      maxPressureValue += 30;
    }
    break;
    case 2: //min Pressure Value
    {
      get_pressure_sensor_value(&initialPressureValue, 256);
      initialPressureValue += 100;
      EepromWriteInt(INITIALVALUEADDRESS, initialPressureValue);
    }
    break;
    case 3: //debug On/Off
    {
      game_mode = false;
      debug_mode = true;
      Serial.println("debug mode " + String(debug_mode));
    }
    break;
    case 4: //game_mode on for sending Game feedback
    {
      debug_mode = false;
      update_spacing = (uint16_t)get_value_from_string(s, ',', 1).toInt();
      game_mode = true;
      Serial.println("game mode " + String(game_mode));
    }
    break;
    case 5: //Turn off Controller Feedback
    {
      debug_mode = false;
      game_mode = false;
      Serial.println("Turning off Feedback");
    }
    break;
    case 6:
    {
      strcpy(lastCommand, s.c_str());
      //Just use a SINUS Curve instead of Sounds
      //Data: sound || volume ||
      volume = (byte)get_value_from_string(s, ',', 1).toInt();

      if (!is_static || force)
      {
        DacAudio.StopAllSounds();

        if (ContactSound != nullptr)
          delete ContactSound;

        currentFile = soundList[3];
        ContactSound = new XT_Wav_Splitable_Class(currentFile->soundFile, 0);
        ContactSound->Volume = volume;
        ContactSound->RepeatForever = false;
        currentState = UP;
        is_static = true;
      }
    }
    break;
    case 7:
    {
      if (currentState != EXPLODE)
      {
        currentState = EXPLODE;

        DacAudio.StopAllSounds();

        if (ContactSound != nullptr)
          delete ContactSound;

        soundsSelection = 4;
        currentFile = soundList[4];
        ContactSound = new XT_Wav_Splitable_Class(currentFile->soundFile, 0);
        ContactSound->RepeatForever = false;
        ContactSound->Volume = 127;
        DacAudio.Play(ContactSound);
      }
    }
    break;
    case 8:
    {
      volume = (byte)get_value_from_string(s, ',', 1).toInt();
      if (currentState != EXPLODE || (ContactSound != NULL && !ContactSound->Playing) )
      {
        currentState = EXPLODE;

        DacAudio.StopAllSounds(); 

        if (ContactSound != nullptr)
          delete ContactSound;

        soundsSelection = 5;
        currentFile = soundList[5];
        ContactSound = new XT_Wav_Splitable_Class(currentFile->soundFile, 0);
        ContactSound->RepeatForever = false;
        ContactSound->Volume = 127;
        DacAudio.Play(ContactSound);
      }
    }
    break;
    }
  }
}

void loop()
{

  ArduinoOTA.handle();

  DacAudio.FillBuffer();
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

  if (Serial.available())
    process_commands(Serial.readStringUntil('\n'), false);

  uint16_t rawValue = analogRead(34);
  uint16_t pressureValue = rawValue;

  if (pressureValue < initialPressureValue)
    pressureValue = 0;
  else
    pressureValue -= initialPressureValue;

  pressureSensor.add(pressureValue);

  int averagePressureValue = pressureSensor.getAverage();

  if (averagePressureValue > maxPressureValue - initialPressureValue)
  {
    averagePressureValue = maxPressureValue - initialPressureValue;
  }

  byte mapped_pressure_value = map(averagePressureValue, 0, maxPressureValue - initialPressureValue, 0, 127);

  ledcWrite(BLUELED, ledValue[mapped_pressure_value]);

  if (currentState != PAUSE && currentState != EXPLODE)
  {
    if (mapped_pressure_value > 10)
      currentState = DOWN;
    else
      currentState = UP;
  }

  if (mapped_pressure_value > 127)
  {
    mapped_pressure_value = 127;
  }
  /*
bool reached_peak = false;
int current_layer = 0;

byte direction_tolerance_counter = 0;
*/
  byte destAmount = materiallayer / 2 + 1;
  byte outvalue = 0;
  //Check if a sound is created
  if (ContactSound != nullptr)
  {
    switch (currentState)
    {
    case UP:
    {
      //When the currentLayer didn't reached the end it shouldn't be played anymore, otherwise play until the end
      if (!is_static)
      {
        current_layer = -1;
        DacAudio.StopAllSounds();
        reached_peak = false;
      }
      else
      {
        reached_peak = false;
      }
      /*
      if (currentLayer <= 0 || currentLayer >= materiallayer || !(currentFile->givesIn))
      {
        currentLayer = -1;
        DacAudio.StopAllSounds();
        firstDown = false;
      }
      else
      {
        if (!ContactSound->Playing)
        {
          ContactSound->Volume = volume;
          ContactSound->SetCurrentPart(materiallayer - (--currentLayer));
          DacAudio.Play(ContactSound);
        }
        ContactSound->Speed = map(currentLayer, materiallayer, -1, 2700, 1800) / 1000.0f;
        ContactSound->Volume =(byte) map(volume, 0, 127, 0, map(currentLayer, materiallayer, -1, 120, 50));
      }
      */
    }
    break;
    case DOWN:
    {
      byte adjustedPressure = map(mapped_pressure_value, 0, 127, 0, destAmount);
      outvalue = adjustedPressure;

      if (!is_static)
      {
        if (currentFile->givesIn)
        {

          if (!reached_peak)
          {

            int layercurrent = map(adjustedPressure, 0, destAmount, 0, (materiallayer - 1) / 2 + 1);

            if (layercurrent > current_layer)
            {
              current_layer = layercurrent;
              if (current_layer == (materiallayer - 1) / 2)
              {
                reached_peak = true;
              }

              ContactSound->Speed = 1.0f;
              ContactSound->Volume = volume;
              ContactSound->SetCurrentPart(current_layer);
              DacAudio.Play(ContactSound);
            }
            else if (layercurrent < current_layer)
            {
              if (direction_tolerance_counter < DirectionTolerance)
              {
                direction_tolerance_counter++;
              }
              else
              {
                direction_tolerance_counter = 0;
                reached_peak = true;
                current_layer = layercurrent;
              }
            }
            else
            {
              direction_tolerance_counter = 0;
            }
          }
          else
          {

            int layercurrent = map(adjustedPressure, destAmount, 0, (materiallayer - 1) / 2 + 1, materiallayer);

            if (layercurrent > current_layer)
            {
              current_layer = layercurrent;
              if (current_layer == (materiallayer - 1))
              {
                reached_peak = false;
              }

              ContactSound->Speed = 1.0f;
              ContactSound->Volume = volume/2;
              ContactSound->SetCurrentPart(current_layer);
              DacAudio.Play(ContactSound);
            }
            else if (layercurrent < current_layer)
            {
              if (direction_tolerance_counter < DirectionTolerance)
              {
                direction_tolerance_counter++;
              }
              else
              {
                direction_tolerance_counter = 0;
                reached_peak = false;
                current_layer = layercurrent;
              }
            }
            else
            {
              direction_tolerance_counter = 0;
            }
          }
        }
        else
        {
          //Apply the Material Strength

          if (!reached_peak)
          {
            int layercurrent = map(adjustedPressure, 0, materiallayer, 0, materiallayer);

            if (layercurrent > current_layer)
            {
              current_layer = layercurrent;
              if (current_layer == (materiallayer - 1) / 2)
              {
                reached_peak = true;
              }

              ContactSound->Speed = 1.0f;
              //ContactSound->Volume = volume;
              ContactSound->Volume = volume;
              ContactSound->SetCurrentPart(current_layer);
              DacAudio.Play(ContactSound);
            }
          }
        }
      }
      else
      {
        if (!ContactSound->Playing && !reached_peak)
        {
          reached_peak = true;
          ContactSound->Speed = 1.0f;
          ContactSound->Volume = volume;
          DacAudio.Play(ContactSound);
        }
      }
    }
    break;
    case PAUSE:
      if (!ContactSound->Playing)
        DacAudio.StopAllSounds();
      break;
    case EXPLODE:
      if (!ContactSound->Playing)
      {
        DacAudio.StopAllSounds();
        currentState = UP;
        process_commands(lastCommand, true);
      }
      break;
    }
  }

  //Game Mode
  if (game_mode)
  {
    unsigned long currentTime = millis();
    if (currentTime - timestamp >= update_spacing)
    {
      timestamp = currentTime;

      if (outvalue != last_adjusted_pressure)
      {
        last_adjusted_pressure = outvalue;
        String debug_message = "DEBUG;" + String(averagePressureValue) + ";" + String(last_adjusted_pressure) + ";";

        if (client)
        {
          if (client.connected())
            client.println(debug_message);
          else
            client.stop();
        }
      }
    }
  }

  //Debug
  if (debug_mode)
  {
    unsigned long currentTime = millis();
    if (currentTime - timestamp >= DEBUGSPACING)
    {
      timestamp = currentTime;
      String debug_message = "DEBUG;" + String(rawValue) + ";" + String(pressureValue) + ";" + String(averagePressureValue) + ";" + String(outvalue) + ";" + String(reached_peak) + ";";
      Serial.println(debug_message);

      if (client)
      {
        if (client.connected())
          client.println(debug_message);
        else
          client.stop();
      }
    }
  }

  DacAudio.FillBuffer();
}