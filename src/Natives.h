#pragma once
#include <amx/amx.h>

namespace Natives
{
	cell AMX_NATIVE_CALL Mongo_Connect(AMX* amx, cell* params);
	
	// String CRUD
	cell AMX_NATIVE_CALL Mongo_Insert(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_Find(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_UpdateOne(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_UpdateMany(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_DeleteOne(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_DeleteMany(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_Count(AMX* amx, cell* params);

	// Indexes
	cell AMX_NATIVE_CALL Mongo_CreateIndex(AMX* amx, cell* params);

	// Builder System
	cell AMX_NATIVE_CALL MongoBuilder_Create(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoBuilder_Destroy(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoBuilder_AddInt(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoBuilder_AddFloat(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoBuilder_AddString(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoBuilder_AddBool(AMX* amx, cell* params);

	// Builder CRUD
	cell AMX_NATIVE_CALL Mongo_InsertBuilder(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_FindBuilder(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_UpdateOneBuilder(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_UpdateManyBuilder(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_DeleteOneBuilder(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_DeleteManyBuilder(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL Mongo_CountBuilder(AMX* amx, cell* params);

	// Advanced Cache Methods
	cell AMX_NATIVE_CALL MongoCache_GetRowCount(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoCache_GetDocument(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoCache_GetExecutionTime(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoCache_GetAffectedCount(AMX* amx, cell* params);

	// BSON Native Getters
	cell AMX_NATIVE_CALL MongoCache_GetInt(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoCache_GetFloat(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoCache_GetString(AMX* amx, cell* params);
	cell AMX_NATIVE_CALL MongoCache_GetBool(AMX* amx, cell* params);
}
