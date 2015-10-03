#include <iostream>
#include <string>
#include <gtk/gtk.h>
#include <lldb/API/LLDB.h>

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

struct DebuggerState {
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

static void
activate (GtkApplication* app,
          gpointer        user_data)
{
    GtkWidget *window;

    DebuggerState* state = (DebuggerState*)user_data;

    lldb::SBSymbol mainsymbol = state->GetSymbol("main");
    std::string mainsFileName = state->GetSymbolsFileName(mainsymbol);

    uint32_t linenumber = state->GetSymbolsLineNumber(mainsymbol);

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "Test");
    gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);

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
    gtk_container_add (GTK_CONTAINER (scrolled), view);
    gtk_stack_add_titled (GTK_STACK (stack), scrolled, "","");

    gchar *contents;
    gsize length;

    if (g_file_get_contents (mainsFileName.c_str(), &contents, &length, NULL))
    {
        GtkTextBuffer *buffer;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
        gtk_text_buffer_set_text (buffer, contents, length);

        PangoFontDescription *font_desc;
        font_desc = pango_font_description_from_string ("monospace 12");
        gtk_widget_modify_font (view, font_desc);
        pango_font_description_free (font_desc);
        /*
        gtk_text_buffer_create_tag (buffer, "font", "font", "fixed", NULL); 
        */

        GtkTextIter start, end;

        gtk_text_buffer_get_start_iter (buffer, &start);
        gtk_text_buffer_get_end_iter(buffer, &end);
        /*
        gtk_text_buffer_apply_tag_by_name (buffer, "font", &start, &end);
        */

        GdkColor color;
        gdk_color_parse("gray",&color);
        gtk_text_buffer_create_tag (buffer, "bold", "weight", PANGO_WEIGHT_BOLD, "background-gdk", &color, NULL);
        gtk_text_buffer_get_iter_at_line(buffer, &start, linenumber);
        gtk_text_buffer_get_iter_at_line(buffer, &end, linenumber+1);
        gtk_text_buffer_apply_tag_by_name (buffer, "bold", &start, &end);

        GtkTextMark *last_pos;
        last_pos = gtk_text_buffer_create_mark (buffer, "last_pos", &end, FALSE);

        scrolltomark* stm = new scrolltomark(view,last_pos);
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
    state.AddTarget(argv[1]);

    app = gtk_application_new ("my.test", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), &state);
    status = g_application_run (G_APPLICATION (app), argc+1, argv+1);
    g_object_unref (app);


    return status;
}

