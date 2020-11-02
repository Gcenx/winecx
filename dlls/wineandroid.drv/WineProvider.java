/*
 * WineProvider class
 *
 * Copyright 2016 Vincent Povirk for CodeWeavers
 */

package org.winehq.wine;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import java.io.IOException;
import java.io.FileOutputStream;
import org.winehq.wine.WineDriver;

public class WineProvider extends ContentProvider {
    private static final int COPYING = 1;

    private static final UriMatcher uri_matcher = new UriMatcher( UriMatcher.NO_MATCH );

    static
    {
        uri_matcher.addURI( "*", "copying", 1 );
    }

    @Override
    public Cursor query( Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder ) {
        Log.i( "Wine", "WineProvider.query "+uri.toString() );
        return null;
    }

    @Override
    public Uri insert( Uri uri, ContentValues values ) {
        return null;
    }

    @Override
    public int update( Uri uri, ContentValues values, String selection, String[] selectionArgs ) {
        return 0;
    }

    @Override
    public int delete( Uri uri, String selection, String[] selectionArgs ) {
        return 0;
    }

    @Override
    public String getType( Uri uri ) {
        Log.i( "wine", "WineProvider.getType "+uri.toString() );
        return null;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public String[] getStreamTypes( Uri uri, String mimeTypeFilter )
    {
        Log.i( "wine", "getStreamTypes "+uri.toString()+" "+mimeTypeFilter );
        return null;
    }

    @Override
    public AssetFileDescriptor openTypedAssetFile( Uri uri, String mimeTypeFilter, Bundle opts )
    {
        Log.i( "wine", "openTypedAssetFile "+uri.toString()+" "+mimeTypeFilter );
        return null;
    }
}
