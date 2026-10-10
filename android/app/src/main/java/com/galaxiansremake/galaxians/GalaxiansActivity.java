package com.galaxiansremake.galaxians;

import org.libsdl.app.SDLActivity;

// SDL's activity runs the game's SDL_main from libmain.so.
public class GalaxiansActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }
}
