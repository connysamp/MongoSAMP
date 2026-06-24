#pragma once
#include <string>
#include <amx/amx.h>
#include <vector>
#include <map>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/builder/basic/document.hpp>

struct AMX_Call {
	AMX* amx;
	std::string callback_name;
	std::string format;
	std::vector<int> int_params;

	// BSON Cache Metadata
	std::vector<bsoncxx::document::value> cache_bson_documents;
	long long execution_time_ms;
	int affected_count;

	// Error Callback Metadata
	bool is_error = false;
	std::string error_message;
};

namespace MongoSystem
{
	void Init();
	void Cleanup();
	void ProcessCallbacks(); // Executes pending amx_Calls
	void PushCallback(const AMX_Call& call); // Push a callback to the queue
	void PushError(AMX* amx, const std::string& error_msg);

	bool Connect(const std::string& uri, const std::string& db);
	
	// String JSON CRUD
	void Insert(const std::string& collection, const std::string& json_data);
	void Find(AMX* amx, const std::string& collection, const std::string& json_filter, const std::string& callback, const std::string& format, std::vector<int> extra_ints);
	void UpdateOne(const std::string& collection, const std::string& json_filter, const std::string& json_update);
	void UpdateMany(AMX* amx, const std::string& collection, const std::string& json_filter, const std::string& json_update, const std::string& callback, const std::string& format, std::vector<int> extra_ints);
	void DeleteOne(const std::string& collection, const std::string& json_filter);
	void DeleteMany(AMX* amx, const std::string& collection, const std::string& json_filter, const std::string& callback, const std::string& format, std::vector<int> extra_ints);
	void Count(AMX* amx, const std::string& collection, const std::string& json_filter, const std::string& callback, const std::string& format, std::vector<int> extra_ints);

	// Index Creation
	void CreateIndex(const std::string& collection, const std::string& field, bool unique);

	// Builder System
	int BuilderCreate();
	void BuilderDestroy(int id);
	void BuilderAddInt(int id, const std::string& key, int val);
	void BuilderAddFloat(int id, const std::string& key, float val);
	void BuilderAddString(int id, const std::string& key, const std::string& val);
	void BuilderAddBool(int id, const std::string& key, bool val);

	// Builder CRUD
	void InsertBuilder(const std::string& collection, int builder_id);
	void FindBuilder(AMX* amx, const std::string& collection, int builder_id, const std::string& callback, const std::string& format, std::vector<int> extra_ints);
	void UpdateOneBuilder(const std::string& collection, int filter_builder_id, int update_builder_id);
	void UpdateManyBuilder(AMX* amx, const std::string& collection, int filter_builder_id, int update_builder_id, const std::string& callback, const std::string& format, std::vector<int> extra_ints);
	void DeleteOneBuilder(const std::string& collection, int filter_builder_id);
	void DeleteManyBuilder(AMX* amx, const std::string& collection, int filter_builder_id, const std::string& callback, const std::string& format, std::vector<int> extra_ints);
	void CountBuilder(AMX* amx, const std::string& collection, int filter_builder_id, const std::string& callback, const std::string& format, std::vector<int> extra_ints);

	// Global Active Cache Methods
	int CacheGetRowCount();
	long long CacheGetExecutionTime();
	std::string CacheGetDocument(int row_idx); // Returns JSON string if needed
	int CacheGetAffectedCount();

	// BSON Native Getters
	int CacheGetInt(int row_idx, const std::string& field_name);
	float CacheGetFloat(int row_idx, const std::string& field_name);
	std::string CacheGetString(int row_idx, const std::string& field_name);
	bool CacheGetBool(int row_idx, const std::string& field_name);
}
