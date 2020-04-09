# HapticShoe
Haptic Shoe Controller Software

## How to install ?
* Download VSCode
* Install the PlattformIO extension (via the build in extension manager)
* Clone this repository
* Open it with VSCode
* Add the folder you cloned this repository to as a Porject in the PlattformIO tab


## Needed libraries
Right now the only one you need to install via PlattformIO's library manager is "RunningMedian"


## How to use it?
Connect the Shoe to your PC and open up a serial terminal (baudrate should ne 115200), then send commands over

## Commands
* 0,granularity,frequency(in Hz),amplitude(0-127),duration(milliseconds) : basic mode e.g.: 0,4,200,80,100
* 1,0 : Calibrate max weight
* 2,0 : Calibrate min weight
* 3,0 : Output sensor values
* 4,0 : Stop outputting sensor values
* 5,frequency(in Hz),amplitude(0-127),duration(milliseconds) : just play a tone e.g.: 0,200,80,100
