#if 0
g++ -std=c++11 -O0 -g -o asynctest $0 `pkg-config --cflags --libs sdl2 gl` -I/usr/lib/llvm-3.6/include/ -L/usr/lib/llvm-3.6/lib/ -llldb-3.6
exit
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/select.h>
#include <termios.h>
#include <vector>
#include <algorithm>

#define ARRAYSIZE(a) ((sizeof((a))/sizeof((a)[0])))

#include "filemap.cc"
#include "lldb.cc"

using std::vector;

typedef struct FileFullOfLines {
    FileMap             *RawText;
    vector<const char*> LinePositions;
} FileFullOfLines;

static const char* NextLine(const char *Text, const char *Extent) {
    if (Text) {
        while (Text != Extent) {
            if (*Text == '\n') {
                Text++;
                if (*Text == '\r') {
                    Text++;
                }
                return Text;
            } else if (*Text == '\r') {
                Text++;
                return Text;
            }
            Text++;
        }
    }
    return Text;
}

vector<const char*> FillInLinePositions(
        const char *Text, const char *Extent) {
    vector<const char*> LinePositions;
    int i;
    while (Text != Extent) {
        LinePositions.push_back(Text);
        Text = NextLine(Text,Extent);
    }
    return LinePositions;
}

static void FileFullOfLinesInitialize(FileFullOfLines *F, FileMap *Map) {
    F->RawText = Map;
    const char *Text = (const char*)Map->Pointer;
    const char *Extent = Text + Map->Size;
    F->LinePositions = FillInLinePositions(Text,Extent);
}

void SetTerminalInput()
{
    struct termios ttystate, ttysave;

    //get the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);
    ttysave = ttystate;
    //turn off canonical mode and echo
    // ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_lflag &= ~(ICANON);
    //minimum of number input read.
    ttystate.c_cc[VMIN] = 1;

    //set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

static const char* TargetExeName = NULL;
static bool TargetIsRunning = false;

static lldb::SBProcess Process;

static void ParseCommandLineOptions(int argc, char *argv[]) {
    for (int i=0;i<argc;i++) {
        if (argv[i][0] == '-') {
        } else {
            TargetExeName = argv[i];
        }
    }
}

typedef struct Breakpoint {
    lldb::SBBreakpoint      Breakpoint;
    bool                    Temporary;
} Breakpoint;

vector<Breakpoint*> BreakpointList;

static bool BreakpointCallback(void *baton, lldb::SBProcess &process, lldb::SBThread &thread, lldb::SBBreakpointLocation &location) {
    Breakpoint *bp = (Breakpoint*)baton;

    if (bp->Temporary) {
        auto n = std::find(BreakpointList.begin(),BreakpointList.end(),bp);
        BreakpointList.erase(n);
        bp->Breakpoint.ClearAllBreakpointSites();
        dbg.target.BreakpointDelete(bp->Breakpoint.GetID());
        delete bp;
    }

    return true;
}

static Breakpoint* CreateBreakpointByName(const char *name, bool Temporary = false) {
    Breakpoint *bp = NULL;
    lldb::SBBreakpoint TestBreakpoint = dbg.target.BreakpointCreateByName(name);
    if (TestBreakpoint.IsValid()) {
        bp = new Breakpoint;

        bp->Breakpoint = TestBreakpoint;
        bp->Breakpoint.SetCallback(BreakpointCallback,bp);
        bp->Temporary = Temporary;
    }

    return bp;
}

static vector<char*> EmptyTokens;
static void Run(vector<char*> Tokens = EmptyTokens) {
    char pwd[1024];
    getcwd(pwd,sizeof(pwd));
    Tokens.insert(Tokens.begin(),(char*)TargetExeName);
    Tokens.push_back(0);
    Process = dbg.target.LaunchSimple((const char**)&Tokens[0],0,pwd);
    TargetIsRunning = true;
}

static void Next() {
    if (!TargetIsRunning) {
        Breakpoint *bp = CreateBreakpointByName("main",true);
        BreakpointList.push_back(bp);
        Run();
    } else {
        auto Thread = Process.GetSelectedThread();
        Thread.StepOver();
    }
}

static void Continue() {
    if (!TargetIsRunning) {
        Run();
    } else {
        Process.Continue();
    }
}

static void AddBreak(vector<char*> Tokens) {
    if (Tokens.size()>1) {
        Breakpoint *bp = CreateBreakpointByName(Tokens[1]);
        BreakpointList.push_back(bp);
    } else {
        printf("Current breakpoints\n");
        for(auto it=BreakpointList.begin();
                it!=BreakpointList.end();
                ++it) {
            auto Path = LLDB_GetBreakpointPath(&dbg,(*it)->Breakpoint,0);
            auto LineNumber = LLDB_GetBreakpointLineNumber(&dbg,(*it)->Breakpoint,0);
            printf("%s:%d\n",Path.c_str(),LineNumber);
        }
    }
}

bool kbhit() {
    struct timeval tv = {0,0};
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(0,&readfds);

    int n;

    if (n = select(1, &readfds, NULL, NULL, &tv) > 0) {
        if (FD_ISSET(0,&readfds)) {
            return true;
        }
    }

    return false;
}

vector<char*> Tokenize(char *Input) {
    vector<char*> Tokens;
    char* Token = strtok(Input," ");
    while (Token) {
        Tokens.push_back(Token);
        Token = strtok(NULL," ");
    }
    return Tokens;
}

struct ConsoleInput {
    char Buffer[128];
    int  Length;
};
bool ProcessConsoleInput(ConsoleInput *CI) {
    int ch = getchar();
    if (EOF != ch) {
        if (ch == 0xa) {
            return true;
        }
        if (CI->Length < sizeof(CI->Buffer)-1) {
            CI->Buffer[CI->Length++] = ch;
            CI->Buffer[CI->Length] = 0;
        }
    }
    return false;
}

typedef enum {
    Command_Unknown,
    Command_Ambiguous,
    Command_Quit,
    Command_Continue,
    Command_SetBreak,
    Command_StepOver,
    Command_Help,
} Command;

typedef struct CommandName {
    const char *Name;
    Command    C;
} CommandName;
CommandName CommandNames[] = {
    {"quit",Command_Quit},
    {"next",Command_StepOver},
    {"break",Command_SetBreak},
    {"continue",Command_Continue},
    {"help",Command_Help},
};
Command GetCommand(vector<char*> Input) {
    if (Input.size()==0) return Command_Unknown;
    Command Result = Command_Unknown;
    bool found;
    for (int i=0;i<ARRAYSIZE(CommandNames);i++) {
        if (0 == strncmp(CommandNames[i].Name,Input[0],strlen(Input[0]))) {
            if (found) {
                Result = Command_Ambiguous;
            } else {
                found = true;
                Result = CommandNames[i].C;
            }
        }
    }
    return Result;
}

int main(int argc, char *argv[]) {
    std::string MainFileName;
    uint32_t MainLineNumber;

    SetTerminalInput();

    ParseCommandLineOptions(argc,argv);

    if (TargetExeName) {
        dbg.dbg.SetAsync(true);
        LLDB_AddTarget(&dbg, TargetExeName);
        auto MainSymbol = LLDB_GetSymbol(&dbg,"main");
        MainFileName = LLDB_GetSymbolsFileName(&dbg, MainSymbol);
        MainLineNumber = LLDB_GetSymbolsLineNumber(MainSymbol);
    }

    FileMap Text;
    MapFile(&Text, MainFileName.c_str());
    FileFullOfLines MainFileThing;
    FileFullOfLinesInitialize(&MainFileThing,&Text);

    auto Listener = dbg.dbg.GetListener();

    ConsoleInput CI;
    CI.Length = 0;
    CI.Buffer[0] = 0;
    bool done = false;
    Command LastCommand = Command_Unknown;
    printf(">");
    fflush(0);
    while (!done) {
        lldb::SBEvent Event;
        if (Listener.PeekAtNextEvent(Event)) {
            Listener.GetNextEvent(Event);
            lldb::SBStream Description;
            Event.GetDescription(Description);
            printf("%s\n",Description.GetData());
            printf("%d\n",Event.GetType());
            switch (Event.GetType()) {
            case 1: { /* State Changed */
                switch (Process.GetState()) {
                case lldb::eStateStopped: {
                    auto LineEntry = LLDB_GetCurrentFrameLocation(Process);
                    auto FileSpec = LineEntry.GetFileSpec();
                    printf("%s:%d >",FileSpec.GetFilename(),LineEntry.GetLine());
                    const char *FileLine = MainFileThing.LinePositions[LineEntry.GetLine()-1];
                    while (*FileLine && *FileLine != '\n') {
                        printf("%c",*FileLine++);
                    }
                    printf("\n");
                } break;
                }
            } break;
            }
        }
        if (kbhit()) {
            if (ProcessConsoleInput(&CI)) {
                vector<char*> Tokens = Tokenize(CI.Buffer);
                Command C = LastCommand;
                if (Tokens.size()) {
                    C = GetCommand(Tokens);
                }
                switch (C) {
                case Command_Quit: {
                    done = true;
                } break;
                case Command_StepOver: {
                    Next();
                } break;
                case Command_Continue: {
                    Continue();
                } break;
                case Command_SetBreak: {
                    AddBreak(Tokens);
                } break;
                case Command_Help: {
                    for (int i=0;i<ARRAYSIZE(CommandNames);i++) {
                        printf("%s\n",CommandNames[i].Name);
                    }
                } break;
                }
                LastCommand = C;
                CI.Length=0;
                CI.Buffer[0] = 0;
                printf(">");
                fflush(0);
            }
        }
    }

    return 0;
}
