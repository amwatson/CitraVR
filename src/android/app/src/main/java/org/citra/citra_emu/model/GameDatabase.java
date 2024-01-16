package org.citra.citra_emu.model;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.net.Uri;
import android.text.TextUtils;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.utils.FileUtil;
import org.citra.citra_emu.utils.Log;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import rx.Observable;

/**
 * A helper class that provides several utilities simplifying interaction with
 * the SQLite database.
 */
public final class GameDatabase extends SQLiteOpenHelper {
    public static final int COLUMN_DB_ID = 0;
    public static final int GAME_COLUMN_PATH = 1;
    public static final int GAME_COLUMN_TITLE = 2;
    public static final int GAME_COLUMN_DESCRIPTION = 3;
    public static final int GAME_COLUMN_REGIONS = 4;
    public static final int GAME_COLUMN_GAME_ID = 5;
    public static final int GAME_COLUMN_COMPANY = 6;
    public static final int FOLDER_COLUMN_PATH = 1;
    public static final String KEY_DB_ID = "_id";
    public static final String KEY_GAME_PATH = "path";
    public static final String KEY_GAME_TITLE = "title";
    public static final String KEY_GAME_DESCRIPTION = "description";
    public static final String KEY_GAME_REGIONS = "regions";
    public static final String KEY_GAME_ID = "game_id";
    public static final String KEY_GAME_COMPANY = "company";
    public static final String KEY_FOLDER_PATH = "path";
    public static final String TABLE_NAME_FOLDERS = "folders";
    public static final String TABLE_NAME_GAMES = "games";
    private static final int DB_VERSION = 2;
    private static final String TYPE_PRIMARY = " INTEGER PRIMARY KEY";
    private static final String TYPE_INTEGER = " INTEGER";
    private static final String TYPE_STRING = " TEXT";

    private static final String CONSTRAINT_UNIQUE = " UNIQUE";

    private static final String SEPARATOR = ", ";

    private static final String SQL_CREATE_GAMES = "CREATE TABLE " + TABLE_NAME_GAMES + "("
            + KEY_DB_ID + TYPE_PRIMARY + SEPARATOR
            + KEY_GAME_PATH + TYPE_STRING + SEPARATOR
            + KEY_GAME_TITLE + TYPE_STRING + SEPARATOR
            + KEY_GAME_DESCRIPTION + TYPE_STRING + SEPARATOR
            + KEY_GAME_REGIONS + TYPE_STRING + SEPARATOR
            + KEY_GAME_ID + TYPE_STRING + SEPARATOR
            + KEY_GAME_COMPANY + TYPE_STRING + ")";

    private static final String SQL_CREATE_FOLDERS = "CREATE TABLE " + TABLE_NAME_FOLDERS + "("
            + KEY_DB_ID + TYPE_PRIMARY + SEPARATOR
            + KEY_FOLDER_PATH + TYPE_STRING + CONSTRAINT_UNIQUE + ")";

    private static final String SQL_DELETE_FOLDERS = "DROP TABLE IF EXISTS " + TABLE_NAME_FOLDERS;
    private static final String SQL_DELETE_GAMES = "DROP TABLE IF EXISTS " + TABLE_NAME_GAMES;
    private final Context mContext;

    public GameDatabase(Context context) {
        // Superclass constructor builds a database or uses an existing one.
        super(context, "games.db", null, DB_VERSION);
        mContext = context;
    }

    @Override
    public void onCreate(SQLiteDatabase database) {
        Log.debug("[GameDatabase] GameDatabase - Creating database...");

        execSqlAndLog(database, SQL_CREATE_GAMES);
        execSqlAndLog(database, SQL_CREATE_FOLDERS);
    }

    @Override
    public void onDowngrade(SQLiteDatabase database, int oldVersion, int newVersion) {
        Log.verbose("[GameDatabase] Downgrades not supporting, clearing databases..");
        execSqlAndLog(database, SQL_DELETE_FOLDERS);
        execSqlAndLog(database, SQL_CREATE_FOLDERS);

        execSqlAndLog(database, SQL_DELETE_GAMES);
        execSqlAndLog(database, SQL_CREATE_GAMES);
    }

    @Override
    public void onUpgrade(SQLiteDatabase database, int oldVersion, int newVersion) {
        Log.info("[GameDatabase] Upgrading database from schema version " + oldVersion + " to " +
                newVersion);

        // Delete all the games
        execSqlAndLog(database, SQL_DELETE_GAMES);
        execSqlAndLog(database, SQL_CREATE_GAMES);
    }

    public void resetDatabase(SQLiteDatabase database) {
        execSqlAndLog(database, SQL_DELETE_FOLDERS);
        execSqlAndLog(database, SQL_CREATE_FOLDERS);

        execSqlAndLog(database, SQL_DELETE_GAMES);
        execSqlAndLog(database, SQL_CREATE_GAMES);
    }

    public void scanLibrary(SQLiteDatabase database) {
        // Start transaction
        database.beginTransaction();
        try {
            // Projection to select only necessary columns
            String[] projection = {KEY_DB_ID, KEY_GAME_PATH};
            Set<String> allowedExtensions = new HashSet<>(Arrays.asList(
                    ".3ds", ".3dsx", ".elf", ".axf", ".cci", ".cxi", ".app", ".rar", ".zip", ".7z", ".torrent", ".tar", ".gz"));

            // Use try-with-resources to ensure that the cursor is closed after usage
            try (Cursor fileCursor = database.query(TABLE_NAME_GAMES, projection, null, null, null, null, null)) {
                while (fileCursor.moveToNext()) {
                    String gamePath = fileCursor.getString(GAME_COLUMN_PATH);
                    if (!FileUtil.Exists(mContext, gamePath)) {
                        database.delete(TABLE_NAME_GAMES, KEY_DB_ID + " = ?", new String[]{Long.toString(fileCursor.getLong(COLUMN_DB_ID))});
                    }
                }
            }

            try (Cursor folderCursor = database.query(TABLE_NAME_FOLDERS, new String[]{KEY_DB_ID, KEY_FOLDER_PATH}, null, null, null, null, null)) {
                while (folderCursor.moveToNext()) {
                    String folderPath = folderCursor.getString(FOLDER_COLUMN_PATH);
                    Uri folder = Uri.parse(folderPath);
                    CheapDocument[] files = FileUtil.listFiles(mContext, folder);
                    if (files.length == 0) {
                        database.delete(TABLE_NAME_FOLDERS, KEY_DB_ID + " = ?", new String[]{Long.toString(folderCursor.getLong(COLUMN_DB_ID))});
                    }
                    addGamesRecursive(database, files, allowedExtensions, 3);
                }
            }

            // If any other operations are needed, add here

            // Commit the transaction
            database.setTransactionSuccessful();
        } finally {
            // End the transaction
            database.endTransaction();
        }
    }

    private void addGamesRecursive(SQLiteDatabase database, CheapDocument[] files, Set<String> allowedExtensions, int depth) {
        if (depth <= 0) {
            return;
        }

        for (CheapDocument file : files) {
            if (file.isDirectory()) {
                CheapDocument[] children = FileUtil.listFiles(mContext, file.getUri());
                addGamesRecursive(database, children, allowedExtensions, depth - 1);
            } else {
                String filename = file.getUri().toString();
                int extensionStart = filename.lastIndexOf('.');
                if (extensionStart > 0) {
                    String fileExtension = filename.substring(extensionStart).toLowerCase();
                    if (allowedExtensions.contains(fileExtension)) {
                        attemptToAddGame(database, filename);
                    }
                }
            }
        }
    }


    private static void attemptToAddGame(SQLiteDatabase database, String filePath) {
        GameInfo gameInfo;
        try {
            gameInfo = new GameInfo(filePath);
        } catch (IOException e) {
            gameInfo = null;
        }

        String name = gameInfo != null ? gameInfo.getTitle() : "";

        // If the game's title field is empty, use the filename.
        if (name.isEmpty()) {
            name = filePath.substring(filePath.lastIndexOf("/") + 1);
        }

        ContentValues game = Game.asContentValues(name,
                filePath.replace("\n", " "),
                gameInfo != null ? gameInfo.getRegions() : "Invalid region",
                filePath,
                filePath,
                gameInfo != null ? gameInfo.getCompany() : "");

        // Try to update an existing game first.
        int rowsMatched = database.update(TABLE_NAME_GAMES,    // Which table to update.
                game,
                // The values to fill the row with.
                KEY_GAME_ID + " = ?",
                // The WHERE clause used to find the right row.
                new String[]{game.getAsString(
                        KEY_GAME_ID)});    // The ? in WHERE clause is replaced with this,
        // which is provided as an array because there
        // could potentially be more than one argument.

        // If update fails, insert a new game instead.
        if (rowsMatched == 0) {
            Log.verbose("[GameDatabase] Adding game: " + game.getAsString(KEY_GAME_TITLE));
            database.insert(TABLE_NAME_GAMES, null, game);
        } else {
            Log.verbose("[GameDatabase] Updated game: " + game.getAsString(KEY_GAME_TITLE));
        }
    }

    public Observable<Cursor> getGames() {
        return Observable.create(subscriber ->
        {
            Log.info("[GameDatabase] Reading games list...");

            SQLiteDatabase database = getReadableDatabase();
            Cursor resultCursor = database.query(
                    TABLE_NAME_GAMES,
                    null,
                    null,
                    null,
                    null,
                    null,
                    KEY_GAME_TITLE + " ASC"
            );

            // Pass the result cursor to the consumer.
            subscriber.onNext(resultCursor);

            // Tell the consumer we're done; it will unsubscribe implicitly.
            subscriber.onCompleted();
        });
    }

    private void execSqlAndLog(SQLiteDatabase database, String sql) {
        Log.verbose("[GameDatabase] Executing SQL: " + sql);
        database.execSQL(sql);
    }
}
