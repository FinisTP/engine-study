package com.umbra.game;

import org.libsdl.app.SDLActivity;

// The game's activity: SDL's activity, loading only libmain.so (SDL is linked into it statically).
public class UmbraActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }
}
