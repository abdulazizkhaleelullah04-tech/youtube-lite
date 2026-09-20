#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <glib.h>

#define APP_ID "io.github.aziz.YouTubeLite"
#define YOUTUBE_URL "https://www.youtube.com/"

typedef struct {
    GtkWidget *window;
    GtkWidget *webview;
} AppState;

static void back_clicked(GtkButton *button, gpointer data) {
    (void)button;
    AppState *s = data;
    if (webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(s->webview))) {
        webkit_web_view_go_back(WEBKIT_WEB_VIEW(s->webview));
    }
}

static void forward_clicked(GtkButton *button, gpointer data) {
    (void)button;
    AppState *s = data;
    if (webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(s->webview))) {
        webkit_web_view_go_forward(WEBKIT_WEB_VIEW(s->webview));
    }
}

static void home_clicked(GtkButton *button, gpointer data) {
    (void)button;
    AppState *s = data;
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(s->webview), YOUTUBE_URL);
}

static void reload_clicked(GtkButton *button, gpointer data) {
    (void)button;
    AppState *s = data;
    webkit_web_view_reload(WEBKIT_WEB_VIEW(s->webview));
}

static void activate(GtkApplication *app, gpointer data) {
    AppState *s = data;

    GtkWidget *window = gtk_application_window_new(app);
    s->window = window;

    gtk_window_set_title(GTK_WINDOW(window), "YouTube Lite");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    // 1. WebContext Initialization with Low Memory Model
    WebKitWebContext *context = webkit_web_context_get_default();

    // RAM optimization for WebKitGTK 6.0
    webkit_web_context_set_cache_model(context, WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

    // Register WebExtension Directory (Loads your ad blocker)
    const char *webext_dir = "/app/lib/webext";
    webkit_web_context_set_web_process_extensions_directory(context, webext_dir);

    // 2. Persistent Session (Saves Login Cookies)
    g_autofree char *data_dir = g_build_filename(g_get_user_data_dir(), "YouTubeLite", NULL);
    g_autofree char *cache_dir = g_build_filename(g_get_user_cache_dir(), "YouTubeLite", NULL);

    WebKitNetworkSession *network_session = webkit_network_session_new(data_dir, cache_dir);

    // 3. Configure WebKit Settings & User Agent
    WebKitSettings *settings = webkit_settings_new();
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_enable_media(settings, TRUE);
    webkit_settings_set_enable_webaudio(settings, TRUE);
    webkit_settings_set_enable_smooth_scrolling(settings, FALSE);

    // Standard Desktop User-Agent prevents Google Sign-In blocking
    webkit_settings_set_user_agent(
        settings, 
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36"
    );

    // 4. Create WebView with NetworkSession and Settings
    GtkWidget *view = GTK_WIDGET(g_object_new(
        WEBKIT_TYPE_WEB_VIEW,
        "web-context", context,
        "network-session", network_session,
        "settings", settings,
        NULL
    ));

    s->webview = view;
    g_object_unref(settings);
    g_object_unref(network_session);

    // 5. Build GTK4 UI Layout
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), root);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(bar, 6);
    gtk_widget_set_margin_end(bar, 6);
    gtk_widget_set_margin_top(bar, 6);
    gtk_widget_set_margin_bottom(bar, 6);
    gtk_box_append(GTK_BOX(root), bar);

    GtkWidget *back = gtk_button_new_with_label("Back");
    GtkWidget *forward = gtk_button_new_with_label("Forward");
    GtkWidget *home = gtk_button_new_with_label("YouTube");
    GtkWidget *reload = gtk_button_new_with_label("Reload");

    gtk_box_append(GTK_BOX(bar), back);
    gtk_box_append(GTK_BOX(bar), forward);
    gtk_box_append(GTK_BOX(bar), home);
    gtk_box_append(GTK_BOX(bar), reload);

    gtk_widget_set_vexpand(s->webview, TRUE);
    gtk_box_append(GTK_BOX(root), s->webview);

    // 6. Connect Navigation Signals
    g_signal_connect(back, "clicked", G_CALLBACK(back_clicked), s);
    g_signal_connect(forward, "clicked", G_CALLBACK(forward_clicked), s);
    g_signal_connect(home, "clicked", G_CALLBACK(home_clicked), s);
    g_signal_connect(reload, "clicked", G_CALLBACK(reload_clicked), s);

    // 7. Load YouTube
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(view), YOUTUBE_URL);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    AppState state = {0};

    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &state);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
