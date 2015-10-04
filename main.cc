#include <iostream>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <lldb/API/LLDB.h>
#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

int bar = 'a'+'\n'+'\t' + 'd';
int foo = 0x1234;

struct MyApplicationWindow {
    GtkApplication parent;

    int width, height;
    bool maximized;
};

struct scrolltomark {
    GtkWidget* view;
    GtkTextMark *last_pos;
    scrolltomark(GtkWidget* w, GtkTextMark* l) :
        view(w), last_pos(l) {
        };
};

gboolean scrolltoline(gpointer user_data) {
    scrolltomark* stm = (scrolltomark*)user_data;

    gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW(stm->view), stm->last_pos, 0.0, TRUE, 0.5, 0.0);
}

struct Debugger {
    Debugger() {
        lldb::SBDebugger::Initialize();
        dbg = lldb::SBDebugger::Create();
    }
    ~Debugger() {
        lldb::SBDebugger::Destroy(dbg);
    }
    lldb::SBDebugger dbg;
    lldb::SBDebugger* operator->() {
        return &dbg;
    }
};

Debugger dbg;

struct LLDBDebuggerHelper {
    lldb::SBTarget AddTarget(const char* fileName) {
        lldb::SBError error;
        targetFileName = fileName;
        target = dbg->CreateTarget(fileName,"x86_64",NULL,NULL,error);
        exe_file_spec = target.GetExecutable();
        module = target.FindModule(fileName);
        return target;
    }
    lldb::SBSymbol GetSymbol(const char* name) {
        return module.FindSymbol(name);
    }
    std::string GetSymbolsFileName(lldb::SBSymbol symbol) {
        auto symbolcu = symbol.GetStartAddress().GetCompileUnit();
        auto symbolfs = symbolcu.GetFileSpec();

        std::string fileName = symbolfs.GetDirectory();
        fileName += "/";
        fileName += symbolfs.GetFilename();

        return fileName;
    }
    uint32_t GetSymbolsLineNumber(lldb::SBSymbol symbol) {
        auto symbolle = symbol.GetStartAddress().GetLineEntry();

        return symbolle.GetLine();
    }
    const char* targetFileName;
    lldb::SBTarget target;
    lldb::SBFileSpec exe_file_spec;
    lldb::SBModule module;
};

const char* KnownKeywords[] = {
    "break",
    "case",
    "continue",
    "default",
    "do",
    "else",
    "for",
    "goto",
    "if",
    "return",
    "sizeof",
    "switch",
    "while",
};

const char* KnownTypes[] = {
    "auto",
    "char",
    "const",
    "double",
    "enum",
    "extern",
    "float",
    "int",
    "long",
    "register",
    "short",
    "signed",
    "static",
    "struct",
    "typedef",
    "union",
    "unsigned",
    "void",
    "volatile",
};

int IsKnownThing(const char** list, int length, const char* start, const char* end) {
    int n = end - start + 1;
    for (int i=0;i<length;i++) {
        if (strncmp(list[i],start,n)==0 &&
            strlen(list[i]) == n) {
            return 1;
        }
    }
    return 0;
}

int IsKnownKeyword(const char* start, const char* end) {
    return IsKnownThing(KnownKeywords,ARRAY_SIZE(KnownKeywords),start,end);
}

int IsKnownType(const char* start, const char* end) {
    return IsKnownThing(KnownTypes,ARRAY_SIZE(KnownTypes),start,end);
}

struct DebuggerState {
    LLDBDebuggerHelper   *lldbHelper;
    PangoFontDescription *base_font_desc;

    GtkTextTag* base_tag;
    GtkTextTag* keyword_tag;
    GtkTextTag* type_tag;
    GtkTextTag* current_line_tag;
    GtkTextTag* syntax_tags[CLEX_first_unused_token];
};

static void
InitializeDebuggerState(DebuggerState* state) {
    assert(state);
    state->base_font_desc = pango_font_description_from_string ("monospace 12");

    GdkRGBA bgcolor, fgcolor;
    gdk_rgba_parse(&bgcolor,"black");
    gdk_rgba_parse(&fgcolor,"gray");

    state->base_tag = gtk_text_tag_new("base");
    g_object_set(state->base_tag, "weight", PANGO_WEIGHT_NORMAL,
            "background-rgba", &bgcolor,
            "foreground-rgba", &fgcolor, NULL);

    state->current_line_tag = gtk_text_tag_new("current_line_tag");
    gdk_rgba_parse(&bgcolor,"#202020");
    g_object_set(state->current_line_tag, "weight", PANGO_WEIGHT_NORMAL,
            "background-rgba", &bgcolor, NULL);

#define CREATETAGI(tag,name,color)                              \
    state->tag = gtk_text_tag_new(name);                        \
    gdk_rgba_parse(&fgcolor,color);                             \
    g_object_set(state->tag,                                    \
            "weight", PANGO_WEIGHT_NORMAL,                      \
            "foreground-rgba", &fgcolor, NULL);

#define CREATETAG(tag,color)                                    \
    CREATETAGI(tag,#tag,color)

    CREATETAG(keyword_tag,"blue");
    CREATETAG(type_tag,"yellow");
    memset(state->syntax_tags,0,sizeof(state->syntax_tags));

#define CREATETOKENTAG(token,color)                             \
    CREATETAGI(syntax_tags[token],#token, color)

    CREATETOKENTAG(CLEX_id,"lightgray");
    CREATETOKENTAG(CLEX_dqstring,"magenta");
    CREATETOKENTAG(CLEX_charlit,"red");
    CREATETOKENTAG(CLEX_intlit,"green");
    CREATETOKENTAG(CLEX_floatlit,"green");
}

static void
DestroyDebuggerState(DebuggerState* state) {
    assert(state);
    pango_font_description_free (state->base_font_desc);
}

static GtkTextBuffer*
CreateSyntaxedTextBuffer(DebuggerState* state, GtkWidget* view, gchar *contents, gsize length)
{
    GtkTextBuffer *buffer;
    GtkTextTagTable *tags;

    gtk_widget_modify_font (view, state->base_font_desc);

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

    tags = gtk_text_buffer_get_tag_table(buffer);
    gtk_text_tag_table_add(tags,state->base_tag);
    gtk_text_tag_table_add(tags,state->keyword_tag);
    gtk_text_tag_table_add(tags,state->type_tag);
    gtk_text_tag_table_add(tags,state->current_line_tag);
    for (int i=0;i<ARRAY_SIZE(state->syntax_tags);i++) {
        if (state->syntax_tags[i]) {
            gtk_text_tag_table_add(tags,state->syntax_tags[i]);
        }
    }

    gtk_text_buffer_set_text(buffer,contents,length);
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);


    stb_lexer lexer;
    char* strings_buffer = (char*)malloc(1<<16);
    stb_c_lexer_init(&lexer,contents,contents+length,strings_buffer,1<<16);
    while (stb_c_lexer_get_token(&lexer)) {
#if 0
        if (lexer.token == CLEX_parse_error) {
            break;
        }
#endif
        GtkTextTag* tag;
        if (state->syntax_tags[lexer.token]) {
            if (lexer.token == CLEX_id)
            {
                if (IsKnownKeyword(lexer.where_firstchar,lexer.where_lastchar)) {
                    tag = state->keyword_tag;
                } else if (IsKnownType(lexer.where_firstchar,lexer.where_lastchar)) {
                    tag = state->type_tag;
                } else {
                    tag = state->syntax_tags[lexer.token];
                }
            }
            else {
                tag = state->syntax_tags[lexer.token];
            }
            gtk_text_buffer_get_iter_at_offset(buffer,&start,lexer.where_firstchar-contents);
#if 0
            if (lexer.token == CLEX_charlit) {
                gtk_text_buffer_get_iter_at_offset(buffer,&end,lexer.where_lastchar-contents);
            } else
#endif
            {
                gtk_text_buffer_get_iter_at_offset(buffer,&end,lexer.where_lastchar-contents+1);
            }
            gtk_text_buffer_apply_tag(buffer,tag,&start,&end);
        }
    }

    return buffer;
}

static gboolean
WindowStateEvent(GtkWidget *widget,
        GdkEventWindowState  *event,
        gpointer   user_data)
{
    MyApplicationWindow* mywindow = (MyApplicationWindow*)user_data;
    mywindow->maximized = (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);
    return FALSE;
}

static gboolean
WindowConfigureEvent(GtkWidget *widget,
        GdkEventConfigure  *event,
        gpointer   user_data)
{
    MyApplicationWindow* mywindow = (MyApplicationWindow*)user_data;
    mywindow->width = event->width;
    mywindow->height = event->height;
    return FALSE;
}

static void
WindowDestroy(GtkWidget* w, gpointer user_data)
{
    MyApplicationWindow* mywindow = (MyApplicationWindow*)user_data;

    GKeyFile* key_file = g_key_file_new();
    g_key_file_set_integer (key_file, "WindowState", "Width", mywindow->width);
    g_key_file_set_integer (key_file, "WindowState", "Height", mywindow->height);
    g_key_file_set_integer (key_file, "WindowState", "Maximized", mywindow->maximized);

    g_key_file_save_to_file (key_file, "state.ini", NULL);

    g_key_file_unref(key_file);
}

static void
LoadWindowSettings(MyApplicationWindow* mywindow)
{
    mywindow->width = mywindow->height = -1;
    mywindow->maximized = FALSE;
    GKeyFile* key_file = g_key_file_new();
    if (!g_key_file_load_from_file(key_file, "state.ini",G_KEY_FILE_NONE,NULL)) return;

    GError* error;

    error  = NULL;
    mywindow->width = g_key_file_get_integer(key_file, "WindowState", "Width", &error);
    if (error != NULL)
    {
        mywindow->width = -1;
    }

    error  = NULL;
    mywindow->height = g_key_file_get_integer(key_file, "WindowState", "Height", &error);
    if (error != NULL)
    {
        mywindow->height = -1;
    }

    error  = NULL;
    mywindow->maximized = g_key_file_get_integer(key_file, "WindowState", "Maximized", &error);
    if (error != NULL)
    {
        mywindow->maximized = 0;
    }

    g_key_file_unref(key_file);
}

static void
activate (GtkApplication* app,
          gpointer        user_data)
{
    GtkWidget *window;

    DebuggerState* state = (DebuggerState*)user_data;

    lldb::SBSymbol mainsymbol = state->lldbHelper->GetSymbol("main");
    std::string mainsFileName = state->lldbHelper->GetSymbolsFileName(mainsymbol);

    uint32_t linenumber = state->lldbHelper->GetSymbolsLineNumber(mainsymbol);

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "Test");
    gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
    MyApplicationWindow* mywindow = new MyApplicationWindow;
    LoadWindowSettings(mywindow);
    gtk_window_set_default_size (GTK_WINDOW (window),
            mywindow->width,
            mywindow->height);

    if (mywindow->maximized)
        gtk_window_maximize (GTK_WINDOW (window));
    g_signal_connect (window, "window-state-event", G_CALLBACK (WindowStateEvent), mywindow);
    g_signal_connect (window, "configure-event", G_CALLBACK (WindowConfigureEvent), mywindow);
    g_signal_connect (window, "destroy", G_CALLBACK (WindowDestroy), mywindow);

    GtkWidget *vbox;
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    GtkWidget *header;
    header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header),"header");
    gtk_box_pack_start(GTK_BOX(vbox),header,FALSE,FALSE,0);
    GtkWidget *stackSwitcher;
    stackSwitcher = gtk_stack_switcher_new();
    GtkWidget *stack;
    stack = gtk_stack_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stackSwitcher),
            GTK_STACK(stack));
    gtk_box_pack_end(GTK_BOX(vbox),stack,TRUE,TRUE,0);

    gtk_container_add(GTK_CONTAINER(window),vbox);

    GtkWidget* scrolled;
    scrolled = gtk_scrolled_window_new(NULL,NULL);
    gtk_widget_show(scrolled);

    gtk_widget_set_hexpand (scrolled, TRUE);
    gtk_widget_set_vexpand (scrolled, TRUE);
    GtkWidget* view;
    view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
    GdkRGBA bgcolor, fgcolor;
    gdk_rgba_parse(&bgcolor,"black");
    gdk_rgba_parse(&fgcolor,"white");
    gtk_widget_override_background_color(view, GTK_STATE_FLAG_NORMAL,&bgcolor);
    gtk_container_add (GTK_CONTAINER (scrolled), view);
    gtk_stack_add_titled (GTK_STACK (stack), scrolled, "","");

    gchar *contents;
    gsize length;

    if (g_file_get_contents (mainsFileName.c_str(), &contents, &length, NULL))
    {
        GtkTextBuffer *buffer;
        buffer = CreateSyntaxedTextBuffer(state,view,contents,length);

        GtkTextIter start,end;
        gtk_text_buffer_get_iter_at_line(buffer, &start, linenumber);
        gtk_text_buffer_get_iter_at_line(buffer, &end, linenumber+1);
        gtk_text_buffer_apply_tag(buffer,state->current_line_tag,&start,&end);

        GtkTextMark *scrollmark;
        scrollmark = gtk_text_buffer_create_mark (buffer, "scrollmark", &start, FALSE);

        scrolltomark* stm = new scrolltomark(view,scrollmark);
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,scrolltoline,stm,NULL);

        g_free (contents);
    }

    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 30);
    gtk_widget_show (view);
    gtk_widget_show_all (window);
}

int
main (int    argc,
      char **argv)
{
    GtkApplication *app;
    int status;

    if (argc < 2) return -1;

    DebuggerState state;
    InitializeDebuggerState(&state);

    LLDBDebuggerHelper lldbHelper;
    state.lldbHelper = &lldbHelper;
    lldbHelper.AddTarget(argv[1]);

    app = gtk_application_new ("my.test", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), &state);
    status = g_application_run (G_APPLICATION (app), argc+1, argv+1);
    g_object_unref (app);


    return status;
}

