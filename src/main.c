#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <glib.h>

#define APP_ID "io.github.aziz.YouTubeLite"
#define YOUTUBE_URL "https://www.youtube.com/"

typedef enum {
    MODE_AUTO = 0,
    MODE_NORMAL,
    MODE_LITE,
    MODE_ULTRA
} AppMode;

typedef struct {
    GtkWidget *window;
    GtkWidget *webview;
    GtkWidget *mode_dropdown;
    AppMode mode;
} AppState;

static AppMode selected_mode(AppState *s)
{
    guint n = gtk_drop_down_get_selected(
        GTK_DROP_DOWN(s->mode_dropdown));

    return n <= 3 ? (AppMode)n : MODE_AUTO;
}

static AppMode automatic_mode(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f)
        return MODE_LITE;

    char line[256];
    unsigned long available_kb = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemAvailable: %lu kB", &available_kb) == 1)
            break;
    }

    fclose(f);

    /*
     * Automatic mode:
     * < 900 MB available  -> Ultra Lite
     * < 2.5 GB available  -> Lite
     * otherwise            -> Normal
     */
    if (available_kb < 900000)
        return MODE_ULTRA;

    if (available_kb < 2500000)
        return MODE_LITE;

    return MODE_NORMAL;
}

static AppMode effective_mode(AppState *s)
{
    if (s->mode == MODE_AUTO)
        return automatic_mode();

    return s->mode;
}

static const char *css_for_mode(AppMode mode)
{
    if (mode == MODE_NORMAL)
        return "";

    if (mode == MODE_ULTRA) {
        return
            "ytd-ad-slot-renderer,"
            "#player-ads,"
            "#masthead-ad,"
            "ytd-display-ad-renderer,"
            "ytd-in-feed-ad-layout-renderer,"
            "ytd-promoted-sparkles-web-renderer,"
            "ytd-banner-promo-renderer,"
            "ytd-comments,"
            "#comments,"
            "ytd-live-chat-frame,"
            "ytd-rich-section-renderer,"
            "ytd-reel-shelf-renderer,"
            "ytd-merch-shelf-renderer"
            "{display:none!important;}";
    }

    return
        "ytd-ad-slot-renderer,"
        "#player-ads,"
        "#masthead-ad,"
        "ytd-display-ad-renderer,"
        "ytd-in-feed-ad-layout-renderer,"
        "ytd-promoted-sparkles-web-renderer,"
        "ytd-banner-promo-renderer"
        "{display:none!important;}";
}

static const char *script_for_mode(AppMode mode)
{
    if (mode == MODE_NORMAL)
        return "";

    if (mode == MODE_ULTRA) {
        return
            "(function(){"
            "const s=["
            "'ytd-ad-slot-renderer',"
            "'#player-ads',"
            "'#masthead-ad',"
            "'ytd-display-ad-renderer',"
            "'ytd-in-feed-ad-layout-renderer',"
            "'ytd-promoted-sparkles-web-renderer',"
            "'ytd-banner-promo-renderer',"
            "'ytd-comments',"
            "'#comments',"
            "'ytd-live-chat-frame'"
            "];"
            "function c(){"
            "s.forEach(x=>document.querySelectorAll(x)"
            ".forEach(e=>e.remove()))"
            "}"
            "c();"
            "new MutationObserver(c).observe("
            "document.documentElement,"
            "{childList:true,subtree:true}"
            ")"
            "})();";
    }

    return
        "(function(){"
        "const s=["
        "'ytd-ad-slot-renderer',"
        "'#player-ads',"
        "'#masthead-ad',"
        "'ytd-display-ad-renderer',"
        "'ytd-in-feed-ad-layout-renderer',"
        "'ytd-promoted-sparkles-web-renderer',"
        "'ytd-banner-promo-renderer'"
        "];"
        "function c(){"
        "s.forEach(x=>document.querySelectorAll(x)"
        ".forEach(e=>e.remove()))"
        "}"
        "c();"
        "new MutationObserver(c).observe("
        "document.documentElement,"
        "{childList:true,subtree:true}"
        ")"
        "})();";
}

static void apply_mode(AppState *s)
{
    AppMode mode = effective_mode(s);

    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(
            WEBKIT_WEB_VIEW(s->webview));

    webkit_user_content_manager_remove_all_style_sheets(manager);
    webkit_user_content_manager_remove_all_scripts(manager);

    const char *css = css_for_mode(mode);

    if (*css) {
        WebKitUserStyleSheet *sheet =
            webkit_user_style_sheet_new(
                css,
                WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                WEBKIT_USER_STYLE_LEVEL_USER,
                NULL,
                NULL);

        webkit_user_content_manager_add_style_sheet(
            manager, sheet);

        webkit_user_style_sheet_unref(sheet);
    }

    const char *js = script_for_mode(mode);

    if (*js) {
        WebKitUserScript *script =
            webkit_user_script_new(
                js,
                WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                NULL,
                NULL);

        webkit_user_content_manager_add_script(
            manager, script);

        webkit_user_script_unref(script);
    }
}

static void mode_changed(
    GObject *object,
    GParamSpec *pspec,
    gpointer data)
{
    (void)object;
    (void)pspec;

    AppState *s = data;

    s->mode = selected_mode(s);

    apply_mode(s);

    webkit_web_view_reload(
        WEBKIT_WEB_VIEW(s->webview));
}

static gboolean decide_policy(
    WebKitWebView *view,
    WebKitPolicyDecision *decision,
    WebKitPolicyDecisionType type,
    gpointer data)
{
    (void)view;
    (void)data;

    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        return FALSE;

    WebKitNavigationPolicyDecision *navigation =
        WEBKIT_NAVIGATION_POLICY_DECISION(decision);

    WebKitNavigationAction *action =
        webkit_navigation_policy_decision_get_navigation_action(
            navigation);

    WebKitURIRequest *request =
        webkit_navigation_action_get_request(action);

    const char *uri =
        webkit_uri_request_get_uri(request);

    if (uri &&
        (g_str_has_prefix(uri, "https://www.youtube.com/") ||
         g_str_has_prefix(uri, "https://youtube.com/") ||
         g_str_has_prefix(uri, "https://m.youtube.com/") ||
         g_str_has_prefix(uri, "https://youtu.be/") ||
         g_str_has_prefix(uri,
                          "https://www.youtube-nocookie.com/"))) {
        return FALSE;
    }

    webkit_policy_decision_ignore(decision);

    return TRUE;
}

static void back_clicked(GtkButton *button, gpointer data)
{
    (void)button;

    AppState *s = data;

    if (webkit_web_view_can_go_back(
            WEBKIT_WEB_VIEW(s->webview))) {
        webkit_web_view_go_back(
            WEBKIT_WEB_VIEW(s->webview));
    }
}

static void forward_clicked(GtkButton *button, gpointer data)
{
    (void)button;

    AppState *s = data;

    if (webkit_web_view_can_go_forward(
            WEBKIT_WEB_VIEW(s->webview))) {
        webkit_web_view_go_forward(
            WEBKIT_WEB_VIEW(s->webview));
    }
}

static void home_clicked(GtkButton *button, gpointer data)
{
    (void)button;

    AppState *s = data;

    webkit_web_view_load_uri(
        WEBKIT_WEB_VIEW(s->webview),
        YOUTUBE_URL);
}

static void reload_clicked(GtkButton *button, gpointer data)
{
    (void)button;

    AppState *s = data;

    webkit_web_view_reload(
        WEBKIT_WEB_VIEW(s->webview));
}

static void activate(
    GtkApplication *app,
    gpointer data)
{
    AppState *s = data;

    GtkWidget *window =
        gtk_application_window_new(app);

    s->window = window;

    gtk_window_set_title(
        GTK_WINDOW(window),
        "YouTube Lite");

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        1200,
        760);

    WebKitWebContext *context =
        webkit_web_context_get_default();

    webkit_web_context_set_cache_model(
        context,
        WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

    webkit_web_context_set_web_process_extensions_directory(
        context,
        "/app/lib/youtube-lite/webextensions");

    WebKitSettings *settings =
        webkit_settings_new();

    webkit_settings_set_enable_javascript(
        settings, TRUE);

    webkit_settings_set_enable_media(
        settings, TRUE);

    webkit_settings_set_enable_webaudio(
        settings, TRUE);

    webkit_settings_set_enable_page_cache(
        settings, TRUE);

    webkit_settings_set_enable_smooth_scrolling(
        settings, FALSE);

    WebKitUserContentManager *content_manager =
        webkit_user_content_manager_new();

    WebKitNetworkSession *network_session =
        webkit_network_session_get_default();

    WebKitWebView *view =
        WEBKIT_WEB_VIEW(g_object_new(
            WEBKIT_TYPE_WEB_VIEW,
            "network-session", network_session,
            "settings", settings,
            "user-content-manager", content_manager,
            NULL));

    s->webview = GTK_WIDGET(view);

    g_object_unref(settings);
    g_object_unref(content_manager);

    g_signal_connect(
        view,
        "decide-policy",
        G_CALLBACK(decide_policy),
        s);

    GtkWidget *root =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0);

    gtk_window_set_child(
        GTK_WINDOW(window),
        root);

    GtkWidget *bar =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            6);

    gtk_widget_set_margin_start(bar, 6);
    gtk_widget_set_margin_end(bar, 6);
    gtk_widget_set_margin_top(bar, 6);
    gtk_widget_set_margin_bottom(bar, 6);

    gtk_box_append(
        GTK_BOX(root),
        bar);

    GtkWidget *back =
        gtk_button_new_with_label("Back");

    GtkWidget *forward =
        gtk_button_new_with_label("Forward");

    GtkWidget *home =
        gtk_button_new_with_label("YouTube");

    GtkWidget *reload =
        gtk_button_new_with_label("Reload");

    gtk_box_append(GTK_BOX(bar), back);
    gtk_box_append(GTK_BOX(bar), forward);
    gtk_box_append(GTK_BOX(bar), home);
    gtk_box_append(GTK_BOX(bar), reload);

    const char *modes[] = {
        "Automatic",
        "Normal",
        "Lite",
        "Ultra Lite",
        NULL
    };

    GtkStringList *list =
        gtk_string_list_new(modes);

    s->mode_dropdown =
        gtk_drop_down_new(
            G_LIST_MODEL(list),
            NULL);

    g_object_unref(list);

    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(s->mode_dropdown),
        0);

    gtk_box_append(
        GTK_BOX(bar),
        s->mode_dropdown);

    g_signal_connect(
        s->mode_dropdown,
        "notify::selected",
        G_CALLBACK(mode_changed),
        s);

    gtk_widget_set_vexpand(
        s->webview,
        TRUE);

    gtk_box_append(
        GTK_BOX(root),
        s->webview);

    g_signal_connect(
        back,
        "clicked",
        G_CALLBACK(back_clicked),
        s);

    g_signal_connect(
        forward,
        "clicked",
        G_CALLBACK(forward_clicked),
        s);

    g_signal_connect(
        home,
        "clicked",
        G_CALLBACK(home_clicked),
        s);

    g_signal_connect(
        reload,
        "clicked",
        G_CALLBACK(reload_clicked),
        s);

    s->mode = MODE_AUTO;

    apply_mode(s);

    webkit_web_view_load_uri(
        view,
        YOUTUBE_URL);

    gtk_window_present(
        GTK_WINDOW(window));
}

int main(int argc, char **argv)
{
    AppState state = {0};

    GtkApplication *app =
        gtk_application_new(
            APP_ID,
            G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(
        app,
        "activate",
        G_CALLBACK(activate),
        &state);

    int status =
        g_application_run(
            G_APPLICATION(app),
            argc,
            argv);

    g_object_unref(app);

    return status;
}
