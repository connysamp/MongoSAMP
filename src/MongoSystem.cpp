#include "MongoSystem.h"
#include <thread>
#include <mutex>
#include <queue>
#include <memory>
#include <iostream>
#include <functional>
#include <chrono>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/basic/document.hpp>

extern void(*logprintf)(char* format, ...);
extern AMX* pFirstAMX; // We will store one AMX to dispatch global errors if needed, though amx is usually passed. 

namespace MongoSystem
{
	struct Task {
		AMX* source_amx; // Used to return errors to the correct script
		std::function<void()> execute;
	};

	std::unique_ptr<mongocxx::instance> inst;
	std::unique_ptr<mongocxx::client> conn;
	std::string db_name;

	// Thread Pool
	const int THREAD_COUNT = 4;
	std::vector<std::thread> worker_threads;
	std::queue<Task> task_queue;
	std::mutex queue_mutex;
	std::condition_variable queue_cv;
	bool running = false;

	// Callback queue
	std::queue<AMX_Call> callback_queue;
	std::mutex callback_mutex;

	// Global Active Cache
	std::vector<bsoncxx::document::value> active_cache_documents;
	long long active_cache_execution_time_ms = 0;
	int active_cache_affected_count = 0;

	// Builder Registry
	int next_builder_id = 1;
	std::map<int, std::unique_ptr<bsoncxx::builder::basic::document>> builders;
	std::mutex builder_mutex;

	// We'll store a global AMX reference for OnMongoError when no specific AMX is provided
	AMX* global_amx = nullptr;

	void PushCallback(const AMX_Call& call)
	{
		std::lock_guard<std::mutex> lock(callback_mutex);
		callback_queue.push(call);
	}

	void PushError(AMX* amx, const std::string& error_msg)
	{
		AMX_Call cb;
		cb.amx = amx ? amx : global_amx;
		if (!cb.amx) return; // Cannot dispatch error without an AMX instance

		cb.callback_name = "OnMongoError";
		cb.is_error = true;
		cb.error_message = error_msg;
		PushCallback(cb);
	}

	void WorkerLoop()
	{
		while (running)
		{
			Task task;
			{
				std::unique_lock<std::mutex> lock(queue_mutex);
				queue_cv.wait(lock, [] { return !task_queue.empty() || !running; });

				if (!running && task_queue.empty()) break;

				task = std::move(task_queue.front());
				task_queue.pop();
			}

			if (task.execute) {
				try {
					task.execute();
				}
				catch (const mongocxx::exception& e) {
					logprintf("[MongoDB] Exception: %s", e.what());
					PushError(task.source_amx, e.what());
				}
				catch (const std::exception& e) {
					logprintf("[MongoDB] Exception: %s", e.what());
					PushError(task.source_amx, e.what());
				}
			}
		}
	}

	void Init()
	{
		inst = std::make_unique<mongocxx::instance>();
		running = true;
		
		for(int i = 0; i < THREAD_COUNT; i++) {
			worker_threads.emplace_back(WorkerLoop);
		}
	}

	void Cleanup()
	{
		running = false;
		queue_cv.notify_all();
		for(auto& t : worker_threads) {
			if (t.joinable()) {
				t.join();
			}
		}
		worker_threads.clear();
		conn.reset();
		inst.reset();
	}

	void ProcessCallbacks()
	{
		std::queue<AMX_Call> local_queue;
		{
			std::lock_guard<std::mutex> lock(callback_mutex);
			std::swap(local_queue, callback_queue);
		}

		while (!local_queue.empty())
		{
			AMX_Call call = std::move(local_queue.front());
			local_queue.pop();

			if (!call.amx || call.callback_name.empty()) continue;

			int amx_idx;
			if (amx_FindPublic(call.amx, call.callback_name.c_str(), &amx_idx) == AMX_ERR_NONE)
			{
				cell retval;

				if (call.is_error)
				{
					// Push (error_message)
					cell amx_addr, *phys_addr;
					amx_Allot(call.amx, call.error_message.length() + 1, &amx_addr, &phys_addr);
					amx_SetString(phys_addr, call.error_message.c_str(), 0, 0, call.error_message.length() + 1);
					amx_Push(call.amx, amx_addr);

					amx_Exec(call.amx, &retval, amx_idx);
					amx_Release(call.amx, amx_addr);
				}
				else
				{
					// Load cache for this callback
					active_cache_documents = std::move(call.cache_bson_documents);
					active_cache_execution_time_ms = call.execution_time_ms;
					active_cache_affected_count = call.affected_count;

					int param_int_idx = call.int_params.size() - 1;

					for (int i = call.format.length() - 1; i >= 0; --i)
					{
						if (call.format[i] == 'd' || call.format[i] == 'i')
						{
							if (param_int_idx >= 0)
							{
								amx_Push(call.amx, call.int_params[param_int_idx--]);
							}
						}
					}

					amx_Exec(call.amx, &retval, amx_idx);

					// Clear cache after callback
					active_cache_documents.clear();
					active_cache_execution_time_ms = 0;
					active_cache_affected_count = 0;
				}
			}
			else
			{
				if(call.is_error) {
					// OnMongoError not declared by the user, ignore silently.
				} else {
					logprintf("[MongoDB] Callback '%s' not found.", call.callback_name.c_str());
				}
			}
		}
	}

	bool Connect(const std::string& uri_str, const std::string& db)
	{
		db_name = db;
		try {
			mongocxx::uri uri{ uri_str };
			conn = std::make_unique<mongocxx::client>(uri);
			
			// Execute PING command to verify connection immediately
			bsoncxx::builder::basic::document ping_cmd{};
			ping_cmd.append(bsoncxx::builder::basic::kvp("ping", 1));
			(*conn)[db_name].run_command(ping_cmd.extract());

			return true;
		} catch(const std::exception& e) {
			logprintf("[MongoDB] Connection Error: %s", e.what());
			conn.reset(); // Destroy faulty connection
			return false;
		}
	}

	void CreateIndex(const std::string& collection, const std::string& field, bool unique)
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ nullptr, [collection, field, unique]() {
			if (!conn) return;
			auto coll = (*conn)[db_name][collection];
			bsoncxx::builder::basic::document index_builder{};
			index_builder.append(bsoncxx::builder::basic::kvp(field, 1));
			
			mongocxx::options::index options{};
			options.unique(unique);

			coll.create_index(index_builder.view(), options);
			logprintf("[MongoDB] Index created on '%s' (Unique: %d)", field.c_str(), unique);
		} });
		queue_cv.notify_one();
	}

	// Builder System

	int BuilderCreate()
	{
		std::lock_guard<std::mutex> lock(builder_mutex);
		int id = next_builder_id++;
		builders[id] = std::make_unique<bsoncxx::builder::basic::document>();
		return id;
	}

	void BuilderDestroy(int id)
	{
		std::lock_guard<std::mutex> lock(builder_mutex);
		builders.erase(id);
	}

	void BuilderAddInt(int id, const std::string& key, int val)
	{
		std::lock_guard<std::mutex> lock(builder_mutex);
		if (builders.count(id)) {
			builders[id]->append(bsoncxx::builder::basic::kvp(key, val));
		}
	}

	void BuilderAddFloat(int id, const std::string& key, float val)
	{
		std::lock_guard<std::mutex> lock(builder_mutex);
		if (builders.count(id)) {
			builders[id]->append(bsoncxx::builder::basic::kvp(key, (double)val));
		}
	}

	void BuilderAddString(int id, const std::string& key, const std::string& val)
	{
		std::lock_guard<std::mutex> lock(builder_mutex);
		if (builders.count(id)) {
			builders[id]->append(bsoncxx::builder::basic::kvp(key, val));
		}
	}

	void BuilderAddBool(int id, const std::string& key, bool val)
	{
		std::lock_guard<std::mutex> lock(builder_mutex);
		if (builders.count(id)) {
			builders[id]->append(bsoncxx::builder::basic::kvp(key, val));
		}
	}

	bsoncxx::document::value GetBuilderValueAndDestroy(int id)
	{
		std::lock_guard<std::mutex> lock(builder_mutex);
		if (builders.count(id)) {
			bsoncxx::document::value doc = builders[id]->extract();
			builders.erase(id);
			return doc;
		}
		return bsoncxx::builder::basic::document{}.extract();
	}

	// Builder CRUD

	void InsertBuilder(const std::string& collection, int builder_id)
	{
		bsoncxx::document::value doc = GetBuilderValueAndDestroy(builder_id);
		
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ global_amx, [collection, doc = std::move(doc)]() {
			if (!conn) return;
			auto coll = (*conn)[db_name][collection];
			coll.insert_one(doc.view());
		} });
		queue_cv.notify_one();
	}

	void FindBuilder(AMX* amx, const std::string& collection, int builder_id, const std::string& callback, const std::string& format, std::vector<int> extra_ints)
	{
		if (!global_amx) global_amx = amx;
		bsoncxx::document::value doc = GetBuilderValueAndDestroy(builder_id);

		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ amx, [amx, collection, filter_val = std::move(doc), callback, format, extra_ints]() {
			if (!conn) return;
			auto start = std::chrono::high_resolution_clock::now();

			auto coll = (*conn)[db_name][collection];
			auto cursor = coll.find(filter_val.view());
			
			std::vector<bsoncxx::document::value> docs;
			for (auto&& doc_it : cursor) {
				docs.push_back(bsoncxx::document::value(doc_it));
			}

			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

			AMX_Call cb;
			cb.amx = amx;
			cb.callback_name = callback;
			cb.format = format;
			cb.int_params = extra_ints;
			cb.cache_bson_documents = std::move(docs);
			cb.execution_time_ms = duration;
			cb.affected_count = cb.cache_bson_documents.size();

			PushCallback(cb);
		} });
		queue_cv.notify_one();
	}

	void UpdateOneBuilder(const std::string& collection, int filter_builder_id, int update_builder_id)
	{
		bsoncxx::document::value filter = GetBuilderValueAndDestroy(filter_builder_id);
		bsoncxx::document::value update = GetBuilderValueAndDestroy(update_builder_id);

		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ global_amx, [collection, filter_val = std::move(filter), update_val = std::move(update)]() {
			if (!conn) return;
			auto coll = (*conn)[db_name][collection];
			
			// Native MQL requires operators like $set. We assume the user handles this in the builder.
			coll.update_one(filter_val.view(), update_val.view());
		} });
		queue_cv.notify_one();
	}

	void UpdateManyBuilder(AMX* amx, const std::string& collection, int filter_builder_id, int update_builder_id, const std::string& callback, const std::string& format, std::vector<int> extra_ints)
	{
		if (!global_amx) global_amx = amx;
		bsoncxx::document::value filter = GetBuilderValueAndDestroy(filter_builder_id);
		bsoncxx::document::value update = GetBuilderValueAndDestroy(update_builder_id);

		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ amx, [amx, collection, filter_val = std::move(filter), update_val = std::move(update), callback, format, extra_ints]() {
			if (!conn) return;
			auto start = std::chrono::high_resolution_clock::now();
			
			auto coll = (*conn)[db_name][collection];
			auto result = coll.update_many(filter_val.view(), update_val.view());
			int count = result ? result->modified_count() : 0;

			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

			if (!callback.empty()) {
				AMX_Call cb;
				cb.amx = amx;
				cb.callback_name = callback;
				cb.format = format;
				cb.int_params = extra_ints;
				cb.affected_count = count;
				cb.execution_time_ms = duration;
				PushCallback(cb);
			}
		} });
		queue_cv.notify_one();
	}

	void DeleteOneBuilder(const std::string& collection, int filter_builder_id)
	{
		bsoncxx::document::value filter = GetBuilderValueAndDestroy(filter_builder_id);

		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ global_amx, [collection, filter_val = std::move(filter)]() {
			if (!conn) return;
			auto coll = (*conn)[db_name][collection];
			coll.delete_one(filter_val.view());
		} });
		queue_cv.notify_one();
	}

	void DeleteManyBuilder(AMX* amx, const std::string& collection, int filter_builder_id, const std::string& callback, const std::string& format, std::vector<int> extra_ints)
	{
		if (!global_amx) global_amx = amx;
		bsoncxx::document::value filter = GetBuilderValueAndDestroy(filter_builder_id);

		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ amx, [amx, collection, filter_val = std::move(filter), callback, format, extra_ints]() {
			if (!conn) return;
			auto start = std::chrono::high_resolution_clock::now();
			
			auto coll = (*conn)[db_name][collection];
			auto result = coll.delete_many(filter_val.view());
			int count = result ? result->deleted_count() : 0;

			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

			if (!callback.empty()) {
				AMX_Call cb;
				cb.amx = amx;
				cb.callback_name = callback;
				cb.format = format;
				cb.int_params = extra_ints;
				cb.affected_count = count;
				cb.execution_time_ms = duration;
				PushCallback(cb);
			}
		} });
		queue_cv.notify_one();
	}

	void CountBuilder(AMX* amx, const std::string& collection, int filter_builder_id, const std::string& callback, const std::string& format, std::vector<int> extra_ints)
	{
		if (!global_amx) global_amx = amx;
		bsoncxx::document::value filter = GetBuilderValueAndDestroy(filter_builder_id);

		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ amx, [amx, collection, filter_val = std::move(filter), callback, format, extra_ints]() {
			if (!conn) return;
			auto start = std::chrono::high_resolution_clock::now();

			auto coll = (*conn)[db_name][collection];
			int count = coll.count_documents(filter_val.view());

			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

			AMX_Call cb;
			cb.amx = amx;
			cb.callback_name = callback;
			cb.format = format;
			cb.int_params = extra_ints;
			cb.affected_count = count;
			cb.execution_time_ms = duration;
			PushCallback(cb);
		} });
		queue_cv.notify_one();
	}


	// String CRUD (Legacy/Hybrid)

	void Insert(const std::string& collection, const std::string& json_data)
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ global_amx, [collection, json_data]() {
			if (!conn) return;
			auto coll = (*conn)[db_name][collection];
			auto doc = bsoncxx::from_json(json_data);
			coll.insert_one(doc.view());
		} });
		queue_cv.notify_one();
	}

	void Find(AMX* amx, const std::string& collection, const std::string& json_filter, const std::string& callback, const std::string& format, std::vector<int> extra_ints)
	{
		if (!global_amx) global_amx = amx;
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ amx, [amx, collection, json_filter, callback, format, extra_ints]() {
			if (!conn) return;
			auto start = std::chrono::high_resolution_clock::now();

			auto coll = (*conn)[db_name][collection];
			auto filter = bsoncxx::from_json(json_filter);
			auto cursor = coll.find(filter.view());
			
			std::vector<bsoncxx::document::value> docs;
			for (auto&& doc : cursor) {
				docs.push_back(bsoncxx::document::value(doc));
			}

			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

			AMX_Call cb;
			cb.amx = amx;
			cb.callback_name = callback;
			cb.format = format;
			cb.int_params = extra_ints;
			cb.cache_bson_documents = std::move(docs);
			cb.execution_time_ms = duration;
			cb.affected_count = cb.cache_bson_documents.size();

			PushCallback(cb);
		} });
		queue_cv.notify_one();
	}

	void UpdateOne(const std::string& collection, const std::string& json_filter, const std::string& json_update)
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ global_amx, [collection, json_filter, json_update]() {
			if (!conn) return;
			auto coll = (*conn)[db_name][collection];
			auto filter = bsoncxx::from_json(json_filter);
			auto update = bsoncxx::from_json(json_update);
			coll.update_one(filter.view(), update.view());
		} });
		queue_cv.notify_one();
	}

	void UpdateMany(AMX* amx, const std::string& collection, const std::string& json_filter, const std::string& json_update, const std::string& callback, const std::string& format, std::vector<int> extra_ints)
	{
		if (!global_amx) global_amx = amx;
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ amx, [amx, collection, json_filter, json_update, callback, format, extra_ints]() {
			if (!conn) return;
			auto start = std::chrono::high_resolution_clock::now();
			
			auto coll = (*conn)[db_name][collection];
			auto filter = bsoncxx::from_json(json_filter);
			auto update = bsoncxx::from_json(json_update);
			auto result = coll.update_many(filter.view(), update.view());
			
			int count = result ? result->modified_count() : 0;

			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

			if (!callback.empty()) {
				AMX_Call cb;
				cb.amx = amx;
				cb.callback_name = callback;
				cb.format = format;
				cb.int_params = extra_ints;
				cb.affected_count = count;
				cb.execution_time_ms = duration;

				PushCallback(cb);
			}
		} });
		queue_cv.notify_one();
	}

	void DeleteOne(const std::string& collection, const std::string& json_filter)
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ global_amx, [collection, json_filter]() {
			if (!conn) return;
			auto coll = (*conn)[db_name][collection];
			auto filter = bsoncxx::from_json(json_filter);
			coll.delete_one(filter.view());
		} });
		queue_cv.notify_one();
	}

	void DeleteMany(AMX* amx, const std::string& collection, const std::string& json_filter, const std::string& callback, const std::string& format, std::vector<int> extra_ints)
	{
		if (!global_amx) global_amx = amx;
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ amx, [amx, collection, json_filter, callback, format, extra_ints]() {
			if (!conn) return;
			auto start = std::chrono::high_resolution_clock::now();
			
			auto coll = (*conn)[db_name][collection];
			auto filter = bsoncxx::from_json(json_filter);
			auto result = coll.delete_many(filter.view());
			
			int count = result ? result->deleted_count() : 0;

			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

			if (!callback.empty()) {
				AMX_Call cb;
				cb.amx = amx;
				cb.callback_name = callback;
				cb.format = format;
				cb.int_params = extra_ints;
				cb.affected_count = count;
				cb.execution_time_ms = duration;

				PushCallback(cb);
			}
		} });
		queue_cv.notify_one();
	}

	void Count(AMX* amx, const std::string& collection, const std::string& json_filter, const std::string& callback, const std::string& format, std::vector<int> extra_ints)
	{
		if (!global_amx) global_amx = amx;
		std::lock_guard<std::mutex> lock(queue_mutex);
		task_queue.push({ amx, [amx, collection, json_filter, callback, format, extra_ints]() {
			if (!conn) return;
			auto start = std::chrono::high_resolution_clock::now();

			auto coll = (*conn)[db_name][collection];
			auto filter = bsoncxx::from_json(json_filter);
			int count = coll.count_documents(filter.view());

			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

			AMX_Call cb;
			cb.amx = amx;
			cb.callback_name = callback;
			cb.format = format;
			cb.int_params = extra_ints;
			cb.affected_count = count;
			cb.execution_time_ms = duration;

			PushCallback(cb);
		} });
		queue_cv.notify_one();
	}

	int CacheGetRowCount() { return active_cache_documents.size(); }
	long long CacheGetExecutionTime() { return active_cache_execution_time_ms; }
	int CacheGetAffectedCount() { return active_cache_affected_count; }

	std::string CacheGetDocument(int row_idx) { 
		if (row_idx >= 0 && row_idx < active_cache_documents.size()) {
			return bsoncxx::to_json(active_cache_documents[row_idx].view());
		}
		return "";
	}

	int CacheGetInt(int row_idx, const std::string& field_name) {
		if (row_idx >= 0 && row_idx < active_cache_documents.size()) {
			try {
				auto view = active_cache_documents[row_idx].view();
				auto ele = view[field_name];
				if (ele) {
					if (ele.type() == bsoncxx::type::k_int32) return ele.get_int32();
					if (ele.type() == bsoncxx::type::k_int64) return (int)ele.get_int64();
					if (ele.type() == bsoncxx::type::k_double) return (int)ele.get_double();
				}
			} catch (...) {}
		}
		return 0;
	}

	float CacheGetFloat(int row_idx, const std::string& field_name) {
		if (row_idx >= 0 && row_idx < active_cache_documents.size()) {
			try {
				auto view = active_cache_documents[row_idx].view();
				auto ele = view[field_name];
				if (ele) {
					if (ele.type() == bsoncxx::type::k_double) return (float)ele.get_double();
					if (ele.type() == bsoncxx::type::k_int32) return (float)ele.get_int32();
					if (ele.type() == bsoncxx::type::k_int64) return (float)ele.get_int64();
				}
			} catch (...) {}
		}
		return 0.0f;
	}

	std::string CacheGetString(int row_idx, const std::string& field_name) {
		if (row_idx >= 0 && row_idx < active_cache_documents.size()) {
			try {
				auto view = active_cache_documents[row_idx].view();
				auto ele = view[field_name];
				if (ele && ele.type() == bsoncxx::type::k_string) {
					return std::string(ele.get_string().value);
				}
			} catch (...) {}
		}
		return "";
	}

	bool CacheGetBool(int row_idx, const std::string& field_name) {
		if (row_idx >= 0 && row_idx < active_cache_documents.size()) {
			try {
				auto view = active_cache_documents[row_idx].view();
				auto ele = view[field_name];
				if (ele && ele.type() == bsoncxx::type::k_bool) {
					return ele.get_bool();
				}
			} catch (...) {}
		}
		return false;
	}
}
