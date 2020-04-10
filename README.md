# HapticShoe
Haptic Shoe Controller Software

## How to install
* Download VSCode
* Install the PlattformIO extension (with the build-in extension manager of VSCode)
* Clone this repository
* Open it with VSCode
* Open the PlattformIO tab in VSCode and add it as PlattformIO project


## Needed libraries
Right now the only one you need to install is "RunningMedian". You can use the build-in library manager of PlattformIO


## How to use it
Connect the Shoe to your PC and open up a serial terminal (baudrate should be 115200) (I'm using the one that's included inside the Arduino IDE)

## Commands
All commands have to end with /n (though you don't have to add it manually since most terminals automatically add it at the end of the line)

Here is a list of the currently available commands:
* 0,granularity,frequency(in Hz),amplitude(0-127),duration(milliseconds) : basic mode e.g.: 0,4,200,80,100
* 1,0 : Calibrate max weight
* 2,0 : Calibrate min weight
* 3,0 : Output sensor values
* 4,0 : Stop outputting sensor values
* 5,frequency(in Hz),amplitude(0-127),duration(milliseconds) : just play a tone e.g.: 0,200,80,100

## Note
If you intend to use the Shoes for a longer time I recommend you to set the volume to something lower then 100 (they do get quite hot if you use it at full volume)
