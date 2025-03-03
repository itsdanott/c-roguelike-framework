/*
* C ROGUELIKE FRAMEWORK ********************************************************

@@@      @@@      @      @@@
@        @ @      @      @
@        @@@      @      @@@
@@@      @  @     @@@    @

* PUBLIC API ******************************************************************/

#ifndef C_ROGUELIKE_FRAMEWORK_H
#define C_ROGUELIKE_FRAMEWORK_H

void CRLF_Log(const char* fmt, ...);
void CRLF_LogWarning(const char* fmt, ...);
void CRLF_LogError(const char* fmt, ...);

typedef struct {
    void (*CRLF_Log)(const char*, ...);
    void (*CRLF_LogWarning)(const char*, ...);
    void (*CRLF_LogError)(const char*, ...);
} CRLF_API;

#endif //C_ROGUELIKE_FRAMEWORK_H
