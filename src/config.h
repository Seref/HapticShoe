#pragma once

#define HOSTNAME "LEFTSHOE"

#define REDLEDPIN 4
#define GREENLEDPIN 2
#define BLUELEDPIN 15

#define REDLED 1
#define GREENLED 2
#define BLUELED 3

#define INITIALVALUEADDRESS 0


const unsigned char PROGMEM ledValue[] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8,
	8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11, 12, 12,
	12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 17, 17, 17,
	18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25,
	26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 36, 36, 37,
	38, 39, 40, 41, 42, 43, 44, 45, 46, 48, 49, 50, 51, 52, 54, 55,
	56, 58, 59, 60, 62, 63, 65, 66, 68, 70, 71, 73, 75, 77, 79, 81,
	83, 85, 87, 89, 91, 93, 95, 98, 100, 102, 105, 107, 110, 113, 115, 118,
	121, 124, 127, 130, 133, 137, 140, 143, 147, 150, 154, 158, 162, 165, 170, 174,
	178, 182, 187, 191, 196, 201, 205, 210, 215, 221, 226, 232, 237, 243, 249, 255};