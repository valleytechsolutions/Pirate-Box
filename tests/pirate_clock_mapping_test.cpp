#include "pirate_clock_mapping.h"
#include <cassert>
#include <cstdio>
int main(){
 const unsigned expected[]={255,0,255,1,8,2,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,255,35,36,37,38,39,40,41,42,43,44,45,46,47,48,0};
 for(unsigned theme=0;theme<47;++theme){assert(piratebox::watchForTheme(theme)==expected[theme]);if(expected[theme]!=255){assert(piratebox::themeForWatch(expected[theme],theme)==theme);auto canonical=piratebox::themeForWatch(expected[theme]);assert(piratebox::squareThemeAllowed(canonical));assert(piratebox::watchForTheme(canonical)==expected[theme]);}}
 for(unsigned face=0;face<256;++face){auto theme=piratebox::themeForWatch(face);if(theme!=255)assert(piratebox::watchForTheme(theme)==face);}
 assert(piratebox::themeForWatch(0)==46&&piratebox::themeForWatch(0,1)==1);
 assert(piratebox::themeForWatch(34)==255&&piratebox::watchForTheme(47)==255&&piratebox::watchForTheme(255)==255);
 puts("PASS every C5 theme/clock mapping, reverse mapping, Green/Pirate alias and excluded/retired IDs");
}
