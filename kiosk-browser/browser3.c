/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2011 Lukasz Slachciak
 * Copyright (C) 2011 Bob Murphy
 * Copyright (C) 2015 Roman Borisov <rborisoff@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>


static void destroyWindowCb(GtkWidget* widget, GtkWidget* window);
static void closeWebViewCb(WebKitWebView* webView, GtkWidget* window);
static void updateSettings(WebKitWebView* webView);
static void reloadViewCb(WebKitWebView* webView, GtkWidget* window);

int main(int argc, char* argv[])
{
    int i, width = 0,
        height = 0;
    char url[128] = "http://localhost:8080";
    char *c;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            continue;

        c = strchr ("whl", argv[i][1]);
        if (c != NULL) {
            if ((i == (argc-1)) || (argv[i+1][0] == '-')) {
                fprintf(stderr, "option %s requires an argument\n", argv[i]);
                break;
            }
        }

        switch (argv[i][1])
        {
            case 'w':
                i++;
                width = atoi(argv[i]);
                printf("view width: %i\n", width);
                break;
            case 'h':
                i++;
                height = atoi(argv[i]);
                printf("view height: %i\n", height);
                break;
            case 'l':
                i++;
                strncpy (url, argv[i], 128);
                printf("url: %s\n", url);
                break;
        }
    }

    // Initialize GTK+
    gtk_init(&argc, &argv);

    // Create an 800x600 window that will contain the browser instance
    GtkWidget *main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(main_window), width, height);

    // Create a browser instance
    WebKitWebView *webView = WEBKIT_WEB_VIEW(webkit_web_view_new());

    // Put the browser area into the main window
    gtk_container_add(GTK_CONTAINER(main_window), GTK_WIDGET(webView));

    // Set up callbacks so that if either the main window or the browser instance is
    // closed, the program will exit
    g_signal_connect(main_window, "destroy", G_CALLBACK(destroyWindowCb), NULL);
    g_signal_connect(webView, "close-web-view", G_CALLBACK(closeWebViewCb), main_window);
    g_signal_connect(webView, "load-error", G_CALLBACK(reloadViewCb), NULL);

//    g_signal_connect(webView, "load-changed", G_CALLBACK(web_view_load_changed), NULL);

//    updateSettings(webView);

    // Load a web page into the browser instance
    webkit_web_view_load_uri(webView, url);

    // Make sure that when the browser area becomes visible, it will get mouse
    // and keyboard events
    gtk_widget_grab_focus(GTK_WIDGET(webView));

    //maximize if no parameters
    if (!width || !height) {
        gtk_window_maximize(GTK_WINDOW(main_window));
        gtk_window_fullscreen(GTK_WINDOW(main_window));
        gtk_window_set_decorated(GTK_WINDOW(main_window), FALSE);
    }

    // Make sure the main window and all its contents are visible
    gtk_widget_show_all(main_window);

    // Run the main GTK+ event loop
    gtk_main();

    return 0;
}

static void updateSettings(WebKitWebView* webView)
{
    WebKitWebSettings *settings = webkit_web_settings_new ();
//    g_object_set (G_OBJECT(settings), "enable-scripts", FALSE, NULL);
    g_object_set (G_OBJECT(settings), "enable-offline-web-application-cache", TRUE, NULL);
    /* Apply the result */
    webkit_web_view_set_settings (WEBKIT_WEB_VIEW(webView), settings);
}

static void reloadViewCb(WebKitWebView* webView, GtkWidget* window)
{
    webkit_web_view_reload (webView);
}

static void destroyWindowCb(GtkWidget* widget, GtkWidget* window)
{
    gtk_main_quit();
}

static void closeWebViewCb(WebKitWebView* webView, GtkWidget* window)
{
    gtk_widget_destroy(window);
}
