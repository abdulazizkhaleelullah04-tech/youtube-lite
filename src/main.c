#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { MODE_NORMAL, MODE_LITE, MODE_ULTRA, MODE_AUTO } PerfMode;

typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *webview;
    GtkWidget *mode_label;
    WebKitNetworkSession *session;
    PerfMode requested_mode;
    PerfMode effective_mode;
    guint auto_timer;
} AppState;

static const char *home_url = "https://www.youtube.com/";
static const char *mode_name(PerfMode mode) {
    switch (mode) { case MODE_ULTRA: return "Ultra Lite"; case MODE_LITE: return "Lite"; case MODE_AUTO: return "Automatic"; default: return "Normal"; }
}
static const char *mode_token(PerfMode mode) {
    switch (mode) { case MODE_ULTRA: return "ultra"; case MODE_NORMAL: return "normal"; default: return "lite"; }
}
static void update_mode_label(AppState *s) {
    char *text = g_strdup_printf("Performance: %s", mode_name(s->requested_mode));
    if (s->requested_mode == MODE_AUTO && s->effective_mode != MODE_AUTO)
        text = g_strdup_printf("Performance: Automatic (%s)", mode_name(s->effective_mode));
    gtk_label_set_text(GTK_LABEL(s->mode_label), text); g_free(text);
}

static void send_mode_to_extension(AppState *s) {
    GVariant *v = g_variant_new_string(mode_token(s->effective_mode));
    WebKitUserMessage *m = webkit_user_message_new("ytlite-set-mode", v);
    webkit_web_context_send_message_to_all_web_extensions(webkit_web_view_get_context(WEBKIT_WEB_VIEW(s->webview)), m, NULL, NULL, NULL);
    g_object_unref(m);
}

static void apply_mode(AppState *s) {
    WebKitSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(s->webview));
    gboolean ultra = s->effective_mode == MODE_ULTRA;
    webkit_settings_set_enable_page_cache(settings, !ultra);
    webkit_settings_set_enable_smooth_scrolling(settings, !ultra);
    send_mode_to_extension(s);
    update_mode_label(s);
}

static gboolean auto_tick(gpointer data) {
    AppState *s = data;
    if (s->requested_mode != MODE_AUTO) return G_SOURCE_CONTINUE;
    guint64 mem = 0;
    GError *error = NULL;
    gchar *contents = NULL;
    gsize len = 0;
    if (g_file_get_contents("/proc/meminfo", &contents, &len, &error)) {
        char *p = strstr(contents, "MemAvailable:");
        if (p) mem = g_ascii_strtoull(p + strlen("MemAvailable:"), NULL, 10) * 1024ULL;
        g_free(contents);
    } else g_clear_error(&error);
    PerfMode target = MODE_LITE;
    if (mem && mem < 900ULL * 1024ULL * 1024ULL) target = MODE_ULTRA;
    else if (mem && mem > 2500ULL * 1024ULL * 1024ULL) target = MODE_NORMAL;
    if (target != s->effective_mode) { s->effective_mode = target; apply_mode(s); }
    return G_SOURCE_CONTINUE;
}

static void set_mode(AppState *s, PerfMode mode) {
    s->requested_mode = mode;
    if (mode != MODE_AUTO) s->effective_mode = mode;
    else s->effective_mode = MODE_LITE;
    apply_mode(s);
    webkit_web_view_reload(WEBKIT_WEB_VIEW(s->webview));
}
static void navigate_home(GtkButton *b, gpointer d){ AppState*s=d; webkit_web_view_load_uri(WEBKIT_WEB_VIEW(s->webview),home_url); }
static void go_back(GtkButton *b, gpointer d){ AppState*s=d; if(webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(s->webview)))webkit_web_view_go_back(WEBKIT_WEB_VIEW(s->webview)); }
static void go_forward(GtkButton *b, gpointer d){ AppState*s=d; if(webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(s->webview)))webkit_web_view_go_forward(WEBKIT_WEB_VIEW(s->webview)); }
static void reload_page(GtkButton *b, gpointer d){ AppState*s=d; webkit_web_view_reload(WEBKIT_WEB_VIEW(s->webview)); }
static void mode_normal(GtkButton*b,gpointer d){set_mode(d,MODE_NORMAL);} static void mode_lite(GtkButton*b,gpointer d){set_mode(d,MODE_LITE);} static void mode_ultra(GtkButton*b,gpointer d){set_mode(d,MODE_ULTRA);} static void mode_auto(GtkButton*b,gpointer d){set_mode(d,MODE_AUTO);}

static gboolean decide_policy(WebKitWebView *view, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data) {
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) return FALSE;
    WebKitNavigationPolicyDecision *nav = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
    WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nav);
    WebKitURIRequest *request = webkit_navigation_action_get_request(action);
    const char *uri = webkit_uri_request_get_uri(request); if (!uri) return FALSE;
    GUri *parsed = g_uri_parse(uri,G_URI_FLAGS_NONE,NULL); const char *host=parsed?g_uri_get_host(parsed):NULL;
    gboolean youtube=host&&(g_str_has_suffix(host,"youtube.com")||g_str_has_suffix(host,"youtube-nocookie.com")||g_str_has_suffix(host,"youtu.be"));
    if(parsed)g_uri_unref(parsed);
    if(!youtube){g_app_info_launch_default_for_uri(uri,NULL,NULL);webkit_policy_decision_ignore(decision);return TRUE;} return FALSE;
}

static void initialize_web_extensions(WebKitWebContext *context, gpointer data) {
    webkit_web_context_set_web_extensions_directory(context, "/app/lib/youtube-lite/webextensions");
}

static void create_ui(AppState *s) {
    s->window=gtk_application_window_new(s->app); gtk_window_set_title(GTK_WINDOW(s->window),"YouTube Lite"); gtk_window_set_default_size(GTK_WINDOW(s->window),1200,760);
    GtkWidget*root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);gtk_window_set_child(GTK_WINDOW(s->window),root);
    GtkWidget*bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);gtk_box_append(GTK_BOX(root),bar);
    const char *icons[] = {"go-previous-symbolic","go-next-symbolic","view-refresh-symbolic","go-home-symbolic"};
    GtkWidget *buttons[4]; for(int i=0;i<4;i++){buttons[i]=gtk_button_new_from_icon_name(icons[i]);gtk_box_append(GTK_BOX(bar),buttons[i]);}
    g_signal_connect(buttons[0],"clicked",G_CALLBACK(go_back),s);g_signal_connect(buttons[1],"clicked",G_CALLBACK(go_forward),s);g_signal_connect(buttons[2],"clicked",G_CALLBACK(reload_page),s);g_signal_connect(buttons[3],"clicked",G_CALLBACK(navigate_home),s);
    GtkWidget*spacer=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);gtk_widget_set_hexpand(spacer,TRUE);gtk_box_append(GTK_BOX(bar),spacer);
    s->mode_label=gtk_label_new(NULL);gtk_box_append(GTK_BOX(bar),s->mode_label);
    GtkWidget*auto_b=gtk_button_new_with_label("Auto"),*normal=gtk_button_new_with_label("Normal"),*lite=gtk_button_new_with_label("Lite"),*ultra=gtk_button_new_with_label("Ultra Lite");
    g_signal_connect(auto_b,"clicked",G_CALLBACK(mode_auto),s);g_signal_connect(normal,"clicked",G_CALLBACK(mode_normal),s);g_signal_connect(lite,"clicked",G_CALLBACK(mode_lite),s);g_signal_connect(ultra,"clicked",G_CALLBACK(mode_ultra),s);
    gtk_box_append(GTK_BOX(bar),auto_b);gtk_box_append(GTK_BOX(bar),normal);gtk_box_append(GTK_BOX(bar),lite);gtk_box_append(GTK_BOX(bar),ultra);

    WebKitWebsiteDataManager *dm=webkit_website_data_manager_new("base-data-directory",g_build_filename(g_get_user_data_dir(),"youtube-lite","data",NULL),"base-cache-directory",g_build_filename(g_get_user_cache_dir(),"youtube-lite","cache",NULL),NULL);
    s->session=webkit_network_session_new("default",dm); g_object_unref(dm);
    WebKitSettings*settings=webkit_settings_new_with_settings("enable-javascript",TRUE,"enable-media",TRUE,"enable-webaudio",TRUE,"enable-accelerated-2d-canvas",TRUE,"enable-page-cache",TRUE,NULL);
    s->webview=webkit_web_view_new_with_settings(settings);webkit_web_view_set_network_session(WEBKIT_WEB_VIEW(s->webview),s->session);g_signal_connect(s->webview,"decide-policy",G_CALLBACK(decide_policy),s);gtk_widget_set_vexpand(s->webview,TRUE);gtk_box_append(GTK_BOX(root),s->webview);
    s->requested_mode=MODE_LITE;s->effective_mode=MODE_LITE;apply_mode(s);webkit_web_view_load_uri(WEBKIT_WEB_VIEW(s->webview),home_url);
    s->auto_timer=g_timeout_add_seconds(5,auto_tick,s);
}
static void activate(GtkApplication*app,gpointer data){AppState*s=g_new0(AppState,1);s->app=app;create_ui(s);gtk_window_present(GTK_WINDOW(s->window));}
int main(int argc,char**argv){GtkApplication*app=gtk_application_new("io.github.aziz.YouTubeLite",G_APPLICATION_DEFAULT_FLAGS);WebKitWebContext*ctx=webkit_web_context_get_default();g_signal_connect(ctx,"initialize-web-extensions",G_CALLBACK(initialize_web_extensions),NULL);g_signal_connect(app,"activate",G_CALLBACK(activate),NULL);int status=g_application_run(G_APPLICATION(app),argc,argv);g_object_unref(app);return status;}
