#include "Natives.h"
#include "MongoSystem.h"
#include <string>
#include <vector>

std::string GetAmxString(AMX* amx, cell amx_addr)
{
	int len = 0;
	cell* addr = NULL;
	amx_GetAddr(amx, amx_addr, &addr);
	amx_StrLen(addr, &len);

	if (len > 0)
	{
		char* text = new char[len + 1];
		amx_GetString(text, addr, 0, len + 1);
		std::string str(text);
		delete[] text;
		return str;
	}
	return std::string();
}

namespace Natives
{
	cell AMX_NATIVE_CALL Mongo_Connect(AMX* amx, cell* params)
	{
		std::string uri = GetAmxString(amx, params[1]);
		std::string db = GetAmxString(amx, params[2]);
		return MongoSystem::Connect(uri, db) ? 1 : 0;
	}

	cell AMX_NATIVE_CALL Mongo_CreateIndex(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		std::string field = GetAmxString(amx, params[2]);
		bool unique = params[3] != 0;
		MongoSystem::CreateIndex(collection, field, unique);
		return 1;
	}

	// ----------------------------------------------------
	// BUILDER SYSTEM
	// ----------------------------------------------------

	cell AMX_NATIVE_CALL MongoBuilder_Create(AMX* amx, cell* params)
	{
		return MongoSystem::BuilderCreate();
	}

	cell AMX_NATIVE_CALL MongoBuilder_Destroy(AMX* amx, cell* params)
	{
		MongoSystem::BuilderDestroy(params[1]);
		return 1;
	}

	cell AMX_NATIVE_CALL MongoBuilder_AddInt(AMX* amx, cell* params)
	{
		int id = params[1];
		std::string key = GetAmxString(amx, params[2]);
		int val = params[3];
		MongoSystem::BuilderAddInt(id, key, val);
		return 1;
	}

	cell AMX_NATIVE_CALL MongoBuilder_AddFloat(AMX* amx, cell* params)
	{
		int id = params[1];
		std::string key = GetAmxString(amx, params[2]);
		float val = amx_ctof(params[3]);
		MongoSystem::BuilderAddFloat(id, key, val);
		return 1;
	}

	cell AMX_NATIVE_CALL MongoBuilder_AddString(AMX* amx, cell* params)
	{
		int id = params[1];
		std::string key = GetAmxString(amx, params[2]);
		std::string val = GetAmxString(amx, params[3]);
		MongoSystem::BuilderAddString(id, key, val);
		return 1;
	}

	cell AMX_NATIVE_CALL MongoBuilder_AddBool(AMX* amx, cell* params)
	{
		int id = params[1];
		std::string key = GetAmxString(amx, params[2]);
		bool val = params[3] != 0;
		MongoSystem::BuilderAddBool(id, key, val);
		return 1;
	}

	// ----------------------------------------------------
	// BUILDER CRUD
	// ----------------------------------------------------

	cell AMX_NATIVE_CALL Mongo_InsertBuilder(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		int builder_id = params[2];
		MongoSystem::InsertBuilder(collection, builder_id);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_FindBuilder(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		int builder_id = params[2];
		std::string callback = GetAmxString(amx, params[3]);
		std::string format = "";
		if (params[0] >= 4 * sizeof(cell)) {
			format = GetAmxString(amx, params[4]);
		}

		std::vector<int> extra_ints;
		if (format.length() > 0 && params[0] >= (4 + format.length()) * sizeof(cell)) {
			for (size_t i = 0; i < format.length(); i++) {
				if (format[i] == 'd' || format[i] == 'i') {
					cell* addr = NULL;
					amx_GetAddr(amx, params[5 + i], &addr);
					extra_ints.push_back(*addr);
				}
			}
		}

		MongoSystem::FindBuilder(amx, collection, builder_id, callback, format, extra_ints);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_UpdateOneBuilder(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		int filter_id = params[2];
		int update_id = params[3];
		MongoSystem::UpdateOneBuilder(collection, filter_id, update_id);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_UpdateManyBuilder(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		int filter_id = params[2];
		int update_id = params[3];
		
		std::string callback = "";
		std::string format = "";
		std::vector<int> extra_ints;
		
		if (params[0] >= 4 * sizeof(cell)) callback = GetAmxString(amx, params[4]);
		if (params[0] >= 5 * sizeof(cell)) format = GetAmxString(amx, params[5]);

		if (format.length() > 0 && params[0] >= (5 + format.length()) * sizeof(cell)) {
			for (size_t i = 0; i < format.length(); i++) {
				if (format[i] == 'd' || format[i] == 'i') {
					cell* addr = NULL;
					amx_GetAddr(amx, params[6 + i], &addr);
					extra_ints.push_back(*addr);
				}
			}
		}

		MongoSystem::UpdateManyBuilder(amx, collection, filter_id, update_id, callback, format, extra_ints);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_DeleteOneBuilder(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		int filter_id = params[2];
		MongoSystem::DeleteOneBuilder(collection, filter_id);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_DeleteManyBuilder(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		int filter_id = params[2];

		std::string callback = "";
		std::string format = "";
		std::vector<int> extra_ints;
		
		if (params[0] >= 3 * sizeof(cell)) callback = GetAmxString(amx, params[3]);
		if (params[0] >= 4 * sizeof(cell)) format = GetAmxString(amx, params[4]);

		if (format.length() > 0 && params[0] >= (4 + format.length()) * sizeof(cell)) {
			for (size_t i = 0; i < format.length(); i++) {
				if (format[i] == 'd' || format[i] == 'i') {
					cell* addr = NULL;
					amx_GetAddr(amx, params[5 + i], &addr);
					extra_ints.push_back(*addr);
				}
			}
		}

		MongoSystem::DeleteManyBuilder(amx, collection, filter_id, callback, format, extra_ints);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_CountBuilder(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		int filter_id = params[2];
		std::string callback = GetAmxString(amx, params[3]);
		std::string format = "";
		if (params[0] >= 4 * sizeof(cell)) {
			format = GetAmxString(amx, params[4]);
		}

		std::vector<int> extra_ints;
		if (format.length() > 0 && params[0] >= (4 + format.length()) * sizeof(cell)) {
			for (size_t i = 0; i < format.length(); i++) {
				if (format[i] == 'd' || format[i] == 'i') {
					cell* addr = NULL;
					amx_GetAddr(amx, params[5 + i], &addr);
					extra_ints.push_back(*addr);
				}
			}
		}

		MongoSystem::CountBuilder(amx, collection, filter_id, callback, format, extra_ints);
		return 1;
	}


	// ----------------------------------------------------
	// STRING CRUD (Retrocompatibilità)
	// ----------------------------------------------------

	cell AMX_NATIVE_CALL Mongo_Insert(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		std::string json_data = GetAmxString(amx, params[2]);
		MongoSystem::Insert(collection, json_data);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_Find(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		std::string json_filter = GetAmxString(amx, params[2]);
		std::string callback = GetAmxString(amx, params[3]);
		std::string format = "";
		if (params[0] >= 4 * sizeof(cell)) {
			format = GetAmxString(amx, params[4]);
		}

		std::vector<int> extra_ints;
		if (format.length() > 0 && params[0] >= (4 + format.length()) * sizeof(cell)) {
			for (size_t i = 0; i < format.length(); i++) {
				if (format[i] == 'd' || format[i] == 'i') {
					cell* addr = NULL;
					amx_GetAddr(amx, params[5 + i], &addr);
					extra_ints.push_back(*addr);
				}
			}
		}

		MongoSystem::Find(amx, collection, json_filter, callback, format, extra_ints);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_UpdateOne(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		std::string json_filter = GetAmxString(amx, params[2]);
		std::string json_update = GetAmxString(amx, params[3]);
		MongoSystem::UpdateOne(collection, json_filter, json_update);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_UpdateMany(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		std::string json_filter = GetAmxString(amx, params[2]);
		std::string json_update = GetAmxString(amx, params[3]);
		
		std::string callback = "";
		std::string format = "";
		std::vector<int> extra_ints;
		
		if (params[0] >= 4 * sizeof(cell)) callback = GetAmxString(amx, params[4]);
		if (params[0] >= 5 * sizeof(cell)) format = GetAmxString(amx, params[5]);

		if (format.length() > 0 && params[0] >= (5 + format.length()) * sizeof(cell)) {
			for (size_t i = 0; i < format.length(); i++) {
				if (format[i] == 'd' || format[i] == 'i') {
					cell* addr = NULL;
					amx_GetAddr(amx, params[6 + i], &addr);
					extra_ints.push_back(*addr);
				}
			}
		}

		MongoSystem::UpdateMany(amx, collection, json_filter, json_update, callback, format, extra_ints);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_DeleteOne(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		std::string json_filter = GetAmxString(amx, params[2]);
		MongoSystem::DeleteOne(collection, json_filter);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_DeleteMany(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		std::string json_filter = GetAmxString(amx, params[2]);

		std::string callback = "";
		std::string format = "";
		std::vector<int> extra_ints;
		
		if (params[0] >= 3 * sizeof(cell)) callback = GetAmxString(amx, params[3]);
		if (params[0] >= 4 * sizeof(cell)) format = GetAmxString(amx, params[4]);

		if (format.length() > 0 && params[0] >= (4 + format.length()) * sizeof(cell)) {
			for (size_t i = 0; i < format.length(); i++) {
				if (format[i] == 'd' || format[i] == 'i') {
					cell* addr = NULL;
					amx_GetAddr(amx, params[5 + i], &addr);
					extra_ints.push_back(*addr);
				}
			}
		}

		MongoSystem::DeleteMany(amx, collection, json_filter, callback, format, extra_ints);
		return 1;
	}

	cell AMX_NATIVE_CALL Mongo_Count(AMX* amx, cell* params)
	{
		std::string collection = GetAmxString(amx, params[1]);
		std::string json_filter = GetAmxString(amx, params[2]);
		std::string callback = GetAmxString(amx, params[3]);
		std::string format = "";
		if (params[0] >= 4 * sizeof(cell)) {
			format = GetAmxString(amx, params[4]);
		}

		std::vector<int> extra_ints;
		if (format.length() > 0 && params[0] >= (4 + format.length()) * sizeof(cell)) {
			for (size_t i = 0; i < format.length(); i++) {
				if (format[i] == 'd' || format[i] == 'i') {
					cell* addr = NULL;
					amx_GetAddr(amx, params[5 + i], &addr);
					extra_ints.push_back(*addr);
				}
			}
		}

		MongoSystem::Count(amx, collection, json_filter, callback, format, extra_ints);
		return 1;
	}


	// ----------------------------------------------------
	// CACHE E GETTERS BSON
	// ----------------------------------------------------

	cell AMX_NATIVE_CALL MongoCache_GetRowCount(AMX* amx, cell* params)
	{
		return MongoSystem::CacheGetRowCount();
	}

	cell AMX_NATIVE_CALL MongoCache_GetDocument(AMX* amx, cell* params)
	{
		int row_idx = params[1];
		std::string doc = MongoSystem::CacheGetDocument(row_idx);

		cell* dest = NULL;
		amx_GetAddr(amx, params[2], &dest);
		amx_SetString(dest, doc.c_str(), 0, 0, params[3]);
		return 1;
	}

	cell AMX_NATIVE_CALL MongoCache_GetExecutionTime(AMX* amx, cell* params)
	{
		return (cell)MongoSystem::CacheGetExecutionTime();
	}

	cell AMX_NATIVE_CALL MongoCache_GetAffectedCount(AMX* amx, cell* params)
	{
		return MongoSystem::CacheGetAffectedCount();
	}

	cell AMX_NATIVE_CALL MongoCache_GetInt(AMX* amx, cell* params)
	{
		int row_idx = params[1];
		std::string field = GetAmxString(amx, params[2]);
		return MongoSystem::CacheGetInt(row_idx, field);
	}

	cell AMX_NATIVE_CALL MongoCache_GetFloat(AMX* amx, cell* params)
	{
		int row_idx = params[1];
		std::string field = GetAmxString(amx, params[2]);
		float val = MongoSystem::CacheGetFloat(row_idx, field);
		return amx_ftoc(val);
	}

	cell AMX_NATIVE_CALL MongoCache_GetString(AMX* amx, cell* params)
	{
		int row_idx = params[1];
		std::string field = GetAmxString(amx, params[2]);
		std::string val = MongoSystem::CacheGetString(row_idx, field);

		cell* dest = NULL;
		amx_GetAddr(amx, params[3], &dest);
		amx_SetString(dest, val.c_str(), 0, 0, params[4]);
		return 1;
	}

	cell AMX_NATIVE_CALL MongoCache_GetBool(AMX* amx, cell* params)
	{
		int row_idx = params[1];
		std::string field = GetAmxString(amx, params[2]);
		return MongoSystem::CacheGetBool(row_idx, field) ? 1 : 0;
	}
}
