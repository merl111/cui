#ifndef CUI_TEST_WINDOWS_DIAGNOSTICS_H
#define CUI_TEST_WINDOWS_DIAGNOSTICS_H
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#pragma comment(lib, "dbghelp.lib")
static LONG WINAPI cui_test_exception(EXCEPTION_POINTERS *exception) {
    HANDLE process=GetCurrentProcess();
    CONTEXT context=*exception->ContextRecord;
    STACKFRAME64 frame={0};
    frame.AddrPC.Offset=context.Rip;
    frame.AddrStack.Offset=context.Rsp;
    frame.AddrFrame.Offset=context.Rbp;
    frame.AddrPC.Mode=frame.AddrStack.Mode=frame.AddrFrame.Mode=AddrModeFlat;
    SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_UNDNAME);
    SymInitialize(process,NULL,TRUE);
    fprintf(stderr,"Unhandled Windows exception 0x%08lx at %p\n",
        exception->ExceptionRecord->ExceptionCode,exception->ExceptionRecord->ExceptionAddress);
    for(unsigned i=0;i<40 && frame.AddrPC.Offset;++i){
        char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME];
        SYMBOL_INFO *symbol=(SYMBOL_INFO *)storage;
        DWORD64 offset=0;DWORD line_offset=0;
        IMAGEHLP_LINE64 line={0};line.SizeOfStruct=sizeof(line);
        symbol->SizeOfStruct=sizeof(*symbol);symbol->MaxNameLen=MAX_SYM_NAME;
        if(SymFromAddr(process,frame.AddrPC.Offset,&offset,symbol))
            fprintf(stderr,"  %s + 0x%llx",symbol->Name,(unsigned long long)offset);
        else fprintf(stderr,"  0x%llx",(unsigned long long)frame.AddrPC.Offset);
        if(SymGetLineFromAddr64(process,frame.AddrPC.Offset,&line_offset,&line))
            fprintf(stderr," (%s:%lu)",line.FileName,line.LineNumber);
        fputc('\n',stderr);
        if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,
            NULL,SymFunctionTableAccess64,SymGetModuleBase64,NULL))break;
    }
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void cui_test_windows_diagnostics(void){SetUnhandledExceptionFilter(cui_test_exception);}
#else
static void cui_test_windows_diagnostics(void){}
#endif
#endif
