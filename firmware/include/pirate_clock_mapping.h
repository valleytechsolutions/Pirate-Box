#pragma once
#include <stdint.h>

namespace piratebox {
constexpr uint8_t InvalidTheme=255;
constexpr bool squareThemeAllowed(unsigned theme){return theme<47&&theme!=0&&theme!=2&&theme!=31;}
constexpr uint8_t watchForTheme(unsigned theme){
 if(!squareThemeAllowed(theme))return InvalidTheme;
 switch(theme){case 1:case 46:return 0;case 3:return 1;case 4:return 8;case 5:return 2;
 case 6:return 9;case 7:return 10;case 8:return 11;case 9:return 12;default:return uint8_t(theme+3);}
}
constexpr uint8_t themeForWatch(unsigned face,unsigned currentTheme=46){
 if(squareThemeAllowed(currentTheme)&&watchForTheme(currentTheme)==face)return uint8_t(currentTheme);
 switch(face){case 0:return 46;case 1:return 3;case 2:return 5;case 8:return 4;
 case 9:return 6;case 10:return 7;case 11:return 8;case 12:return 9;
 default:return face>=13&&face<=48&&face!=34?uint8_t(face-3):InvalidTheme;}
}
}
