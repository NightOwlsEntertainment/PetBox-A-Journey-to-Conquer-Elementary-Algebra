/*
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program:  Loop, watching keystrokes
   Note that you need to call SDL_PollEvent() or SDL_WaitEvent() to
   pump the event loop and catch keystrokes.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_Quit();
    exit(rc);
}

static void
print_string(char **text, size_t *maxlen, const char *fmt, ...)
{
    int len;
    va_list ap;

    va_start(ap, fmt);
    len = SDL_vsnprintf(*text, *maxlen, fmt, ap);
    if (len > 0) {
        *text += len;
        if ( ((size_t) len) < *maxlen ) {
            *maxlen -= (size_t) len;
        } else {
            *maxlen = 0;
        }
    }
    va_end(ap);
}

static void
print_modifiers(char **text, size_t *maxlen)
{
    int mod;
    print_string(text, maxlen, " modifiers:");
    mod = SDL_GetModState();
    if (!mod) {
        print_string(text, maxlen, " (none)");
        return;
    }
    if (mod & KMOD_LSHIFT)
        print_string(text, maxlen, " LSHIFT");
    if (mod & KMOD_RSHIFT)
        print_string(text, maxlen, " RSHIFT");
    if (mod & KMOD_LCTRL)
        print_string(text, maxlen, " LCTRL");
    if (mod & KMOD_RCTRL)
        print_string(text, maxlen, " RCTRL");
    if (mod & KMOD_LALT)
        print_string(text, maxlen, " LALT");
    if (mod & KMOD_RALT)
        print_string(text, maxlen, " RALT");
    if (mod & KMOD_LGUI)
        print_string(text, maxlen, " LGUI");
    if (mod & KMOD_RGUI)
        print_string(text, maxlen, " RGUI");
    if (mod & KMOD_NUM)
        print_string(text, maxlen, " NUM");
    if (mod & KMOD_CAPS)
        print_string(text, maxlen, " CAPS");
    if (mod & KMOD_MODE)
        print_string(text, maxlen, " MODE");
}

static void
PrintKey(SDL_Keysym * sym, SDL_bool pressed, SDL_bool repeat)
{
    char message[512];
    char *spot;
    size_t left;

    spot = message;
    left = sizeof(message);

    /* Print the keycode, name and state */
    if (sym->sym) {
        print_string(&spot, &left,
                "Key %s:  scancode %d = %s, keycode 0x%08X = %s ",
                pressed ? "pressed " : "released",
                sym->scancode,
                SDL_GetScancodeName(sym->scancode),
                sym->sym, SDL_GetKeyName(sym->sym));
    } else {
        print_string(&spot, &left,
                "Unknown Key (scancode %d = %s) %s ",
                sym->scancode,
                SDL_GetScancodeName(sym->scancode),
                pressed ? "pressed " : "released");
    }
    print_modifiers(&spot, &left);
    if (repeat) {
        print_string(&spot, &left, " (repeat)");
    }
    SDL_Log("%s\n", message);
}

static void
PrintText(char *text)
{
    char *spot, expanded[1024];

    expanded[0] = '\0';
    for ( spot = text; *spot; ++spot )
    {
        size_t length = SDL_strlen(expanded);
        SDL_snprintf(expanded + length, sizeof(expanded) - length, "\\x%.2x", (unsigned char)*spot);
    }
    SDL_Log("Text (%s): \"%s%s\"\n", expanded, *text == '"' ? "\\" : "", text);
}

int
main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Event event;
    int done;
	
	/* Enable standard application logging */
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return (1);
    }

    /* Set 640x480 video mode */
    window = SDL_CreateWindow("CheckKeys Test",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              640, 480, 0);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create 640x480 window: %s\n",
                SDL_GetError());
        quit(2);
    }

#if __IPHONEOS__
    /* Creating the context creates the view, which we need to show keyboard */
    SDL_GL_CreateContext(window);
#endif

    SDL_StartTextInput();

    /* Watch keystrokes */
    done = 0;
    while (!done) {
        /* Check for events */
        SDL_WaitEvent(&event);
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
			PrintKey(&event.key.keysym, (event.key.state == SDL_PRESSED) ? SDL_TRUE : SDL_FALSE, (event.key.repeat) ? SDL_TRUE : SDL_FALSE);
            break;
        case SDL_TEXTINPUT:
            PrintText(event.text.text);
            break;
        case SDL_MOUSEBUTTONDOWN:
            /* Any button press quits the app... */
        case SDL_QUIT:
            done = 1;
            break;
        default:
            break;
        }
    }

    SDL_Quit();
    return (0);
}

/* vi: set ts=4 sw=4 expandtab: */
