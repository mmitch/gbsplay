// Defined Color Codes

/* Text attributes */
/*        0    All attributes off */
/*        1    Bold on */
/*        4    Underscore (on monochrome display adapter only) */
/*        5    Blink on */
/*        7    Reverse video on */
/*        8    Concealed on */

/*     Foreground colors */
/*        30    Black */
/*        31    Red */
/*        32    Green */
/*        33    Yellow */
/*        34    Blue */
/*        35    Magenta */
/*        36    Cyan */
/*        37    White */

/*     Background colors */
/*        40    Black */
/*        41    Red */
/*        42    Green */
/*        43    Yellow */
/*        44    Blue */
/*        45    Magenta */
/*        46    Cyan */
/*        47    White */


// Color Stuff:
#define NORMAL() printf("%c[0m", 27);
/*
  All of the following
  colors are bold, I'm going to also write the non-bolded
  versions as well.
*/
// BOLD() is actually blue.
#define BOLD() printf("%c[1;34m", 27);
#define RED() printf("%c[1;31m",27);
#define GREEN() printf("%c[1;32m",27);
#define YELLOW() printf("%c[1;33m",27);
#define CYAN() printf("%c[1;36m",27);
#define MAGENTA() printf("%c[1;35m",27);

// The following is the non-bolded versions of the ones above
#define NONBOLDBLUE() printf("%c[0;34m", 27);
#define NONBOLDRED() printf("%c[0;31m", 27);
#define NONBOLDGREEN() printf("%c[0;32m", 27);
#define NONBOLDYELLOW() printf("%c[0;33m", 27);
#define NONBOLDCYAN() printf("%c[0;36m", 27);
#define NONBOLDMAGENTA() printf("%c[0;35m", 27);
