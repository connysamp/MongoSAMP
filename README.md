# MongoSAMP

MongoSAMP is a next-generation, extreme-performance MongoDB plugin for San Andreas Multiplayer (SA:MP). 
It connects your SA:MP server to any MongoDB database (including MongoDB Atlas) utilizing the official C++ `mongocxx` driver. 

Built with the "End-Game" architecture, MongoSAMP features a **4-worker ThreadPool** for fully asynchronous database operations, a native **BSON Query Builder**, and an ultra-fast **BSON Cache Extractor**, ensuring 0 server lag even under massive loads.

---

## Features
- **Asynchronous ThreadPool**: All CRUD queries are dispatched to a queue handled by 4 parallel background threads. Your server main thread never blocks.
- **Synchronous Secure Connection**: Connection checks are made instantly upon server start. If the database is unreachable, it immediately halts, preventing cascading failures.
- **BSON Query Builder**: Construct MongoDB queries natively in PAWN without relying on messy JSON string formatting.
- **Native BSON Extraction**: Read data directly from the C++ cache memory to PAWN via native functions (`MongoCache_GetInt`, `MongoCache_GetString`, etc.).
- **Global Error Catching**: Catch disconnects, syntax errors, and query failures automatically via a global PAWN callback `OnMongoError`.

---

## Installation

1. Copy `MongoSAMP.dll` (Windows) or `MongoSAMP.so` (Linux) to your `plugins/` directory.
2. Add `MongoSAMP` to your `server.cfg` under `plugins`.
3. Copy `mongo.inc` to your `pawno/include/` directory.
4. Include it in your gamemode/filterscript: `#include <mongo>`

---

## Connection

Connect to your MongoDB server (or Atlas Cluster) inside `OnGameModeInit`. The connection establishes a synchronous ping check. It returns `true` if successful, or `false` if it failed.

```pawn
public OnGameModeInit()
{
    new bool:connected = Mongo_Connect("mongodb+srv://user:pass@cluster0.mongodb.net", "my_database");
    if(!connected)
    {
        print("CRITICAL ERROR: Failed to connect to MongoDB!");
        SendRconCommand("exit");
        return 1;
    }
    print("Connected to MongoDB successfully!");
    return 1;
}
```

---

## Global Error Handler

You don't need to check for errors after every query. Simply define the global callback `OnMongoError` in your script. If any asynchronous thread fails a query or drops connection, it will trigger this callback.

```pawn
forward OnMongoError(const message[]);
public OnMongoError(const message[])
{
    printf("[MongoDB Error]: %s", message);
}
```

---

## BSON Query Builder (Recommended)

MongoSAMP introduces a robust builder API. Instead of formatting JSON strings (which is prone to syntax errors and escaping issues), you can build BSON documents natively.

### Creating a Builder
```pawn
new b = MongoBuilder_Create();
MongoBuilder_AddInt(b, "account_id", 1234);
MongoBuilder_AddString(b, "username", "PlayerOne");
MongoBuilder_AddFloat(b, "health", 100.0);
MongoBuilder_AddBool(b, "is_admin", true);
```

### Executing Builder Queries
Once you pass a builder to a CRUD function, it is **automatically destroyed** and freed from memory. You do not need to call `MongoBuilder_Destroy` manually unless you created a builder and decided not to use it.

```pawn
// Insert
Mongo_InsertBuilder("users", b);

// Find
Mongo_FindBuilder("users", b, "OnUserDataLoaded", "d", playerid);

// Delete
Mongo_DeleteOneBuilder("users", b);
```

---

## Reading Data (BSON Cache System)

Whenever you execute a `Find` query, you pass a PAWN callback. MongoSAMP will execute the query in the background and trigger your callback once the data is ready. The results are stored in a temporary high-speed cache that lives **only** for the duration of the callback.

### Example: Fetching a user
```pawn
// Create filter
new filter = MongoBuilder_Create();
MongoBuilder_AddString(filter, "username", "PlayerOne");

// Search in "users" collection, trigger "OnUserLoad" and pass `playerid`
Mongo_FindBuilder("users", filter, "OnUserLoad", "d", playerid);
```

### Extracting the data
```pawn
forward OnUserLoad(playerid);
public OnUserLoad(playerid)
{
    new rows = MongoCache_GetRowCount();
    if(rows == 0)
    {
        SendClientMessage(playerid, -1, "Account not found!");
        return 1;
    }

    // Extracting fields from Row 0 (the first document)
    new account_id = MongoCache_GetInt(0, "account_id");
    new Float:health = MongoCache_GetFloat(0, "health");
    new bool:is_admin = MongoCache_GetBool(0, "is_admin");
    
    new username[24];
    MongoCache_GetString(0, "username", username, sizeof(username));

    printf("Loaded %s (ID: %d)", username, account_id);
    return 1;
}
```

---

## JSON Queries (Legacy/Hybrid Support)

If you prefer using `format()` to construct standard JSON strings (especially useful for complex MongoDB operators like `$set`, `$inc`, `$push`), MongoSAMP fully supports string-based CRUD operations.

```pawn
new filter[64], update[128];
format(filter, sizeof(filter), "{\"username\": \"%s\"}", playerName);
format(update, sizeof(update), "{\"$set\": {\"score\": %d, \"money\": %d}}", score, money);

// Update a specific user
Mongo_UpdateOne("users", filter, update);
```

---

## Indexes

To optimize performance on frequently searched fields (like usernames), you can easily create database indexes directly from PAWN during startup.

```pawn
// Create a unique index on the "username" field inside the "users" collection
Mongo_CreateIndex("users", "username", true);
```

---

## Full API Reference (mongo.inc)

### Connection & Setup
- `native bool:Mongo_Connect(const uri[], const database[]);`
- `native Mongo_CreateIndex(const collection[], const field[], bool:unique = false);`

### Builder API
- `native MongoBuilder_Create();`
- `native MongoBuilder_Destroy(builder_id);`
- `native MongoBuilder_AddInt(builder_id, const key[], val);`
- `native MongoBuilder_AddFloat(builder_id, const key[], Float:val);`
- `native MongoBuilder_AddString(builder_id, const key[], const val[]);`
- `native MongoBuilder_AddBool(builder_id, const key[], bool:val);`

### Builder CRUD
- `native Mongo_InsertBuilder(const collection[], builder_id);`
- `native Mongo_FindBuilder(const collection[], builder_id, const callback[], const format[] = "", {Float, _}:...);`
- `native Mongo_UpdateOneBuilder(const collection[], filter_builder_id, update_builder_id);`
- `native Mongo_UpdateManyBuilder(const collection[], filter_builder_id, update_builder_id, const callback[] = "", const format[] = "", {Float, _}:...);`
- `native Mongo_DeleteOneBuilder(const collection[], filter_builder_id);`
- `native Mongo_DeleteManyBuilder(const collection[], filter_builder_id, const callback[] = "", const format[] = "", {Float, _}:...);`
- `native Mongo_CountBuilder(const collection[], filter_builder_id, const callback[], const format[] = "", {Float, _}:...);`

### JSON String CRUD
- `native Mongo_Insert(const collection[], const json_data[]);`
- `native Mongo_Find(const collection[], const json_filter[], const callback[], const format[] = "", {Float, _}:...);`
- `native Mongo_UpdateOne(const collection[], const json_filter[], const json_update[]);`
- `native Mongo_UpdateMany(const collection[], const json_filter[], const json_update[], const callback[] = "", const format[] = "", {Float, _}:...);`
- `native Mongo_DeleteOne(const collection[], const json_filter[]);`
- `native Mongo_DeleteMany(const collection[], const json_filter[], const callback[] = "", const format[] = "", {Float, _}:...);`
- `native Mongo_Count(const collection[], const json_filter[], const callback[], const format[] = "", {Float, _}:...);`

### Cache Getters
- `native MongoCache_GetRowCount();`
- `native MongoCache_GetAffectedCount();`
- `native MongoCache_GetExecutionTime();`
- `native MongoCache_GetDocument(row_idx, dest[], max_len = sizeof(dest));`
- `native MongoCache_GetInt(row_idx, const field[]);`
- `native Float:MongoCache_GetFloat(row_idx, const field[]);`
- `native MongoCache_GetString(row_idx, const field[], dest[], max_len = sizeof(dest));`
- `native bool:MongoCache_GetBool(row_idx, const field[]);`
