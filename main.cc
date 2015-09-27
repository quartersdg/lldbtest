#include <iostream>
#include <string>
#include <gtk/gtk.h>
#include <lldb/API/LLDB.h>

struct Debugger {
    Debugger() {
        lldb::SBDebugger::Initialize();
        dbg = lldb::SBDebugger::Create();
    }
    ~Debugger() {
        lldb::SBDebugger::Destroy(dbg);
    }
    lldb::SBDebugger dbg;
};

struct DebuggerState {
    lldb::SDBTarget AddTarget(const char* fileName) {
        lldb::SBError error;
        targetFileName = fileName;
        target = dbg.CreateTarget(fileName,"x86_64",NULL,NULL,error);
        exe_file_spec target.GetExecutable();
    }
    const char* targetFileName;
    lldb::SDBTarget target;
    lldb::SBFileSpec exe_file_spec(g_objectname,true);
    lldb::SDBModule module;
};


static void
activate (GtkApplication* app,
          gpointer        user_data)
{
    GtkWidget *window;

    DebuggerState* state = (DebuggerState*)user_data;

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
    gtk_widget_show (view);
    gtk_container_add (GTK_CONTAINER (scrolled), view);
    gtk_stack_add_titled (GTK_STACK (stack), scrolled, "","");

    gchar *contents;
    gsize length;

    std::cout << "wtf?" << filetoedit.c_str() << std::endl;
    if (g_file_get_contents (filetoedit.c_str(), &contents, &length, NULL))
    {
        GtkTextBuffer *buffer;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
        gtk_text_buffer_set_text (buffer, contents, length);
        g_free (contents);
    }

    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 30);
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
    state.AddTarget(state.argv[1]);
    lldb::SBModule module(target.FindModule(g_objectname));
    auto symbol = module.FindSymbol("main");

    std::cout << symbol.GetName() << ":" << std::hex << symbol.GetStartAddress().GetFileAddress() << std::endl;

    auto symbolcu = symbol.GetStartAddress().GetCompileUnit();
    auto symbolfs = symbolcu.GetFileSpec();

    filetoedit = symbolfs.GetDirectory();
    filetoedit += "/";
    filetoedit += symbolfs.GetFilename();

    std::cout << filetoedit << std::endl;

    app = gtk_application_new ("my.test", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), &state);
    status = g_application_run (G_APPLICATION (app), argc+1, argv+1);
    g_object_unref (app);


    return status;
}

