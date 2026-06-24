#if defined _mongo_included
	#endinput
#endif
#define _mongo_included

// --------------------------------------------------
// Inizializzazione
// --------------------------------------------------
native bool:Mongo_Connect(const uri[], const database[]);

// --------------------------------------------------
// Index Management
// --------------------------------------------------
native Mongo_CreateIndex(const collection[], const field[], bool:unique = false);

// --------------------------------------------------
// BSON Query Builder System (Metodo Consigliato)
// --------------------------------------------------
native MongoBuilder_Create();
native MongoBuilder_Destroy(builder_id);
native MongoBuilder_AddInt(builder_id, const key[], val);
native MongoBuilder_AddFloat(builder_id, const key[], Float:val);
native MongoBuilder_AddString(builder_id, const key[], const val[]);
native MongoBuilder_AddBool(builder_id, const key[], bool:val);

native Mongo_InsertBuilder(const collection[], builder_id);
native Mongo_FindBuilder(const collection[], builder_id, const callback[], const format[] = "", {Float, _}:...);
native Mongo_UpdateOneBuilder(const collection[], filter_builder_id, update_builder_id);
native Mongo_UpdateManyBuilder(const collection[], filter_builder_id, update_builder_id, const callback[] = "", const format[] = "", {Float, _}:...);
native Mongo_DeleteOneBuilder(const collection[], filter_builder_id);
native Mongo_DeleteManyBuilder(const collection[], filter_builder_id, const callback[] = "", const format[] = "", {Float, _}:...);
native Mongo_CountBuilder(const collection[], filter_builder_id, const callback[], const format[] = "", {Float, _}:...);

// --------------------------------------------------
// String JSON CRUD (Retrocompatibilità)
// --------------------------------------------------
native Mongo_Insert(const collection[], const json_data[]);
native Mongo_Find(const collection[], const json_filter[], const callback[], const format[] = "", {Float, _}:...);
native Mongo_UpdateOne(const collection[], const json_filter[], const json_update[]);
native Mongo_UpdateMany(const collection[], const json_filter[], const json_update[], const callback[] = "", const format[] = "", {Float, _}:...);
native Mongo_DeleteOne(const collection[], const json_filter[]);
native Mongo_DeleteMany(const collection[], const json_filter[], const callback[] = "", const format[] = "", {Float, _}:...);
native Mongo_Count(const collection[], const json_filter[], const callback[], const format[] = "", {Float, _}:...);

// --------------------------------------------------
// Sistema di Cache (da usare DENTRO le callback)
// --------------------------------------------------
native MongoCache_GetRowCount();
native MongoCache_GetDocument(row_idx, dest[], max_len = sizeof(dest));
native MongoCache_GetExecutionTime();
native MongoCache_GetAffectedCount();

// --------------------------------------------------
// BSON Native Getters (Estrazione Diretta Cache)
// --------------------------------------------------
native MongoCache_GetInt(row_idx, const field[]);
native Float:MongoCache_GetFloat(row_idx, const field[]);
native MongoCache_GetString(row_idx, const field[], dest[], max_len = sizeof(dest));
native bool:MongoCache_GetBool(row_idx, const field[]);

// --------------------------------------------------
// Error Handling Callback Globale
// --------------------------------------------------
forward OnMongoError(const message[]);
