#include <amx/amx.h>
#include <plugincommon.h>
#include "Natives.h"
#include "MongoSystem.h"

typedef void(*logprintf_t)(char* format, ...);
logprintf_t logprintf;

extern void *pAMXFunctions;
AMX* pFirstAMX = nullptr;

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
	return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData)
{
	pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
	logprintf = (logprintf_t)ppData[PLUGIN_DATA_LOGPRINTF];

	MongoSystem::Init();

	logprintf("MongoSAMP by conny loaded.");
	return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
	MongoSystem::Cleanup();
	logprintf("MongoDB SA:MP Plugin Unloaded.");
}

AMX_NATIVE_INFO PluginNatives[] =
{
	{ "Mongo_Connect", Natives::Mongo_Connect },
	
	// String CRUD
	{ "Mongo_Insert", Natives::Mongo_Insert },
	{ "Mongo_Find", Natives::Mongo_Find },
	{ "Mongo_UpdateOne", Natives::Mongo_UpdateOne },
	{ "Mongo_UpdateMany", Natives::Mongo_UpdateMany },
	{ "Mongo_DeleteOne", Natives::Mongo_DeleteOne },
	{ "Mongo_DeleteMany", Natives::Mongo_DeleteMany },
	{ "Mongo_Count", Natives::Mongo_Count },

	// Indexes
	{ "Mongo_CreateIndex", Natives::Mongo_CreateIndex },

	// Builder System
	{ "MongoBuilder_Create", Natives::MongoBuilder_Create },
	{ "MongoBuilder_Destroy", Natives::MongoBuilder_Destroy },
	{ "MongoBuilder_AddInt", Natives::MongoBuilder_AddInt },
	{ "MongoBuilder_AddFloat", Natives::MongoBuilder_AddFloat },
	{ "MongoBuilder_AddString", Natives::MongoBuilder_AddString },
	{ "MongoBuilder_AddBool", Natives::MongoBuilder_AddBool },

	// Builder CRUD
	{ "Mongo_InsertBuilder", Natives::Mongo_InsertBuilder },
	{ "Mongo_FindBuilder", Natives::Mongo_FindBuilder },
	{ "Mongo_UpdateOneBuilder", Natives::Mongo_UpdateOneBuilder },
	{ "Mongo_UpdateManyBuilder", Natives::Mongo_UpdateManyBuilder },
	{ "Mongo_DeleteOneBuilder", Natives::Mongo_DeleteOneBuilder },
	{ "Mongo_DeleteManyBuilder", Natives::Mongo_DeleteManyBuilder },
	{ "Mongo_CountBuilder", Natives::Mongo_CountBuilder },

	// Cache
	{ "MongoCache_GetRowCount", Natives::MongoCache_GetRowCount },
	{ "MongoCache_GetDocument", Natives::MongoCache_GetDocument },
	{ "MongoCache_GetExecutionTime", Natives::MongoCache_GetExecutionTime },
	{ "MongoCache_GetAffectedCount", Natives::MongoCache_GetAffectedCount },

	// BSON Getters
	{ "MongoCache_GetInt", Natives::MongoCache_GetInt },
	{ "MongoCache_GetFloat", Natives::MongoCache_GetFloat },
	{ "MongoCache_GetString", Natives::MongoCache_GetString },
	{ "MongoCache_GetBool", Natives::MongoCache_GetBool },
	{ 0, 0 }
};

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx)
{
	if (!pFirstAMX) pFirstAMX = amx;
	return amx_Register(amx, PluginNatives, -1);
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx)
{
	if (pFirstAMX == amx) pFirstAMX = nullptr;
	return AMX_ERR_NONE;
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick()
{
	MongoSystem::ProcessCallbacks();
}
