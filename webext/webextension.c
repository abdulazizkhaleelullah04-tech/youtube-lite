#include <webkit/webkit-web-process-extension.h>
#include <glib.h>

G_MODULE_EXPORT void
webkit_web_process_extension_initialize(
WebKitWebProcessExtension *extension);

static gboolean should_block(const char *uri)
{
if (!uri)
return FALSE;

```
static const char *blocked_domains[] = {
    "doubleclick.net",
    "googlesyndication.com",
    "googleadservices.com",
    "adservice.google.com",
    "pagead2.googlesyndication.com",
    "imasdk.googleapis.com",
    "ads.youtube.com",
    "ad.doubleclick.net",
    NULL
};

for (int i = 0; blocked_domains[i] != NULL; ++i) {
    if (g_strstr_len(uri, -1, blocked_domains[i]))
        return TRUE;
}

return FALSE;
```

}

static gboolean send_request(
WebKitWebPage *page,
WebKitURIRequest *request,
WebKitURIResponse *redirected_response,
gpointer user_data)
{
(void)page;
(void)redirected_response;
(void)user_data;

```
const char *uri =
    webkit_uri_request_get_uri(request);

return should_block(uri);
```

}

static void page_created(
WebKitWebProcessExtension *extension,
WebKitWebPage *page,
gpointer user_data)
{
(void)extension;
(void)user_data;

```
g_signal_connect(
    page,
    "send-request",
    G_CALLBACK(send_request),
    NULL);
```

}

G_MODULE_EXPORT void
webkit_web_process_extension_initialize(
WebKitWebProcessExtension *extension)
{
g_signal_connect(
extension,
"page-created",
G_CALLBACK(page_created),
NULL);
}
