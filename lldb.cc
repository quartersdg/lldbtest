#include <iostream>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <lldb/API/LLDB.h>
#if 0
#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#endif
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

struct Debugger {
    Debugger() {
        lldb::SBDebugger::Initialize();
        dbg = lldb::SBDebugger::Create();
    }
    ~Debugger() {
        lldb::SBDebugger::Destroy(dbg);
    }
    lldb::SBDebugger dbg;
    lldb::SBTarget target;
    lldb::SBFileSpec exe_file_spec;
    lldb::SBModule module;
};

Debugger dbg;

lldb::SBTarget LLDB_AddTarget(Debugger *dbg, const char* fileName) {
    lldb::SBError error;
    dbg->target = dbg->dbg.CreateTarget(fileName,"x86_64",NULL,NULL,error);
    dbg->exe_file_spec = dbg->target.GetExecutable();
    dbg->module = dbg->target.FindModule(fileName);
    return dbg->target;
}
lldb::SBSymbol LLDB_GetSymbol(Debugger *dbg, const char* name) {
    return dbg->module.FindSymbol(name);
}
std::string LLDB_GetSymbolsFileName(Debugger *dbg, lldb::SBSymbol symbol) {
    auto symbolcu = symbol.GetStartAddress().GetCompileUnit();
    auto symbolfs = symbolcu.GetFileSpec();

    std::string fileName = symbolfs.GetDirectory();
    fileName += "/";
    fileName += symbolfs.GetFilename();

    return fileName;
}
uint32_t LLDB_GetSymbolsLineNumber(lldb::SBSymbol symbol) {
    auto symboladdress = symbol.GetStartAddress();
    auto symbolle = symboladdress.GetLineEntry();

    return symbolle.GetLine();
}

lldb::SBBreakpoint LLDB_SetBreakpoint(Debugger *dbg, const char *symbol) {
    return dbg->target.BreakpointCreateByName(symbol);
}

bool LLDB_ReadMemory(lldb::SBProcess &process, uint64_t Address, uint8_t *Buffer, size_t NumberBytes) {
    lldb::SBError Error;
    lldb::StateType State;
    State = process.GetState();
    if (State != lldb::eStateStopped) {
        return false;
    }
    process.ReadMemory(Address,Buffer,NumberBytes,Error);
    return true;
}

lldb::SBLineEntry LLDB_GetBreakpointLineEntry(Debugger *dbg, lldb::SBBreakpoint &bp, int index) {
    lldb::SBLineEntry LineEntry;
    auto BreakpointLocation = bp.GetLocationAtIndex(0);
    if (BreakpointLocation.IsValid()) {
        auto Address = BreakpointLocation.GetAddress();
        if (Address.IsValid()) {
            LineEntry = Address.GetLineEntry();
        }
    }
    return LineEntry;
}

lldb::SBFileSpec LLDB_GetBreakpointFileSpec(Debugger *dbg, lldb::SBBreakpoint &bp, int index) {
    lldb::SBFileSpec FileSpec;
    auto LineEntry = LLDB_GetBreakpointLineEntry(dbg,bp,index);
    if (LineEntry.IsValid()) {
        FileSpec = LineEntry.GetFileSpec();
    }
    return FileSpec;
}

std::string LLDB_GetBreakpointPath(Debugger *dbg, lldb::SBBreakpoint &bp, int index) {
    std::string Path;
    auto FileSpec = LLDB_GetBreakpointFileSpec(dbg,bp,index);
    if (FileSpec.IsValid()) {
        Path = FileSpec.GetFilename();
    }
    return Path;
}

int LLDB_GetBreakpointLineNumber(Debugger *dbg, lldb::SBBreakpoint &bp, int index) {
    int LineNumber = 0;
    auto LineEntry = LLDB_GetBreakpointLineEntry(dbg,bp,index);
    if (LineEntry.IsValid()) {
        LineNumber = LineEntry.GetLine();
    }
    return LineNumber;
}

lldb::SBLineEntry LLDB_GetCurrentFrameLocation(lldb::SBProcess &Process) {
    lldb::SBLineEntry LineEntry;

    auto Thread = Process.GetSelectedThread();
    if (Thread.IsValid()) {
        auto Frame = Thread.GetSelectedFrame();
        if (Frame.IsValid()) {
            LineEntry = Frame.GetLineEntry();
        }
    }

    return LineEntry;
}
