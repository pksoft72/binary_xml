#ifndef ANSI_H_
#define ANSI_H_

    #define ANSI_RESET      "\033[0m"
    #define ANSI_RESET_LF       "\033[0m\n"

    #define ANSI_BOLD       "\033[1m"
    #define ANSI_WHITE_NORMAL   "\033[37m"
    #define ANSI_WHITE_HIGH     "\033[97m"
    #define ANSI_BLACK_BRIGHT   "\033[90m"

    #define ANSI_RED_BRIGHT     "\033[91m"
    #define ANSI_GREEN_BRIGHT   "\033[92m"
    #define ANSI_YELLOW_BRIGHT  "\033[93m"
    #define ANSI_MAGENTA_BRIGHT "\033[95m"
    #define ANSI_BLUE_BRIGHT    "\033[96m"

    #define ANSI_RED_DARK       "\033[31m"
    #define ANSI_GREEN_DARK     "\033[32m"
    #define ANSI_YELLOW_DARK    "\033[33m"
    #define ANSI_MAGENTA_DARK   "\033[35m"
    #define ANSI_BLUE_DARK      "\033[36m"

//const char *ANSI_CursorPosition(int line0,int col0);
    #define ANSI_ERASE_DISPLAY  "\033[2J"

    #define ANSI_WINDOW_CAPTION "\033]2;"
    #define ANSI_WINDOW_CAPTION_END "\007"
    
    #define ANSI_WHITE_ON_RED   "\033[37;41m"
    #define ANSI_WHITE_ON_GREEN "\033[37;42m"
    #define ANSI_WHITE_ON_MAGENTA "\033[37;45m"


#endif
