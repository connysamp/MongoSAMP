#include <a_samp>
#include <mongo>

#define DIALOG_REGISTER 1000
#define DIALOG_LOGIN    1001

new VehicleDB_ID[MAX_VEHICLES];

main()
{
	print("\n----------------------------------");
	print(" MongoSAMP Test Gamemode Loaded");
	print("----------------------------------\n");
}

public OnGameModeInit()
{
	// Synchronous MongoDB connection with check
	new bool:connected = Mongo_Connect("mongodb+srv://username:password@cluster.mongodb.net", "testdb");
	if(connected)
		print("[MongoDB] Connection established");
	else
		print("[MongoDB] Connection failed");
	
	Mongo_Find("vehicles", "{}", "OnVehiclesLoad");
	
	SetGameModeText("MongoSAMP Test");
	AddPlayerClass(0, 1958.3783, 1343.1572, 15.3746, 269.1425, 0, 0, 0, 0, 0, 0);
	return 1;
}

public OnPlayerConnect(playerid)
{
	new name[MAX_PLAYER_NAME];
	GetPlayerName(playerid, name, sizeof(name));
	
	new query[256];
	format(query, sizeof(query), "{\"name\":\"%s\"}", name);
	
	// Search for the account in background
	Mongo_Find("users", query, "OnAccountCheck", "d", playerid);
	return 1;
}

forward OnAccountCheck(playerid);
public OnAccountCheck(playerid)
{
	new rows = MongoCache_GetRowCount();
	
	if(rows > 0)
	{
		// Account exists, prompt for login
		ShowPlayerDialog(playerid, DIALOG_LOGIN, DIALOG_STYLE_PASSWORD, "Login", "Welcome back! Enter your password:", "Login", "Exit");
	}
	else
	{
		// Account does not exist, prompt for registration
		ShowPlayerDialog(playerid, DIALOG_REGISTER, DIALOG_STYLE_INPUT, "Registration", "Welcome! Choose a password to register:", "Register", "Exit");
	}
	return 1;
}

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
	if(dialogid == DIALOG_REGISTER)
	{
		if(!response) return Kick(playerid);
		
		if(strlen(inputtext) < 4)
		{
			SendClientMessage(playerid, -1, "Password must be at least 4 characters long.");
			ShowPlayerDialog(playerid, DIALOG_REGISTER, DIALOG_STYLE_INPUT, "Registration", "Welcome! Choose a password to register:", "Register", "Exit");
			return 1;
		}
		
		new name[MAX_PLAYER_NAME];
		GetPlayerName(playerid, name, sizeof(name));
		
		new query[512];
		format(query, sizeof(query), "{\"name\":\"%s\", \"password\":\"%s\", \"money\": 5000, \"level\": 1}", name, inputtext);
		
		// Insert user asynchronously
		Mongo_Insert("users", query);
		
		SendClientMessage(playerid, -1, "Registration complete! You are now logged in.");
		GivePlayerMoney(playerid, 5000);
		SetPlayerScore(playerid, 1);
		SpawnPlayer(playerid);
		return 1;
	}
	
	if(dialogid == DIALOG_LOGIN)
	{
		if(!response) return Kick(playerid);
		
		new name[MAX_PLAYER_NAME];
		GetPlayerName(playerid, name, sizeof(name));
		
		new query[256];
		// Fetch the entire user document for BSON feature demonstration
		format(query, sizeof(query), "{\"name\":\"%s\"}", name);
		
		// Store input password temporarily to check against BSON data later
		SetPVarString(playerid, "tmpPass", inputtext);
		
		Mongo_Find("users", query, "OnLoginCheck", "d", playerid);
		return 1;
	}
	return 0;
}

forward OnLoginCheck(playerid);
public OnLoginCheck(playerid)
{
	if(MongoCache_GetRowCount() > 0)
	{
		new pass_db[64], pass_input[64];
		MongoCache_GetString(0, "password", pass_db, sizeof(pass_db));
		GetPVarString(playerid, "tmpPass", pass_input, sizeof(pass_input));
		
		if(strcmp(pass_db, pass_input, false) == 0)
		{
			// Password correct! Load BSON data
			new money = MongoCache_GetInt(0, "money");
			new level = MongoCache_GetInt(0, "level");
			
			GivePlayerMoney(playerid, money);
			SetPlayerScore(playerid, level);
			
			SendClientMessage(playerid, -1, "Login successful!");
			SpawnPlayer(playerid);
		}
		else
		{
			SendClientMessage(playerid, -1, "Incorrect password!");
			ShowPlayerDialog(playerid, DIALOG_LOGIN, DIALOG_STYLE_PASSWORD, "Login", "Incorrect password! Try again:", "Login", "Exit");
		}
	}
	return 1;
}

public OnPlayerSpawn(playerid)
{
	SetPlayerPos(playerid, 1958.3783, 1343.1572, 15.3746);
	return 1;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
	if(strcmp(cmdtext, "/test", true) == 0)
	{
		// Fetch all users (empty filter = {})
		Mongo_Find("users", "{}", "OnUserListFetch", "d", playerid);
		return 1;
	}
	
	if(strfind(cmdtext, "/vcreate", true) == 0)
	{
		new model = 411; // default infernus
		if(strlen(cmdtext) > 9) model = strval(cmdtext[9]);
		if(model < 400 || model > 611) return SendClientMessage(playerid, -1, "Invalid model!");
		
		new Float:x, Float:y, Float:z, Float:a;
		GetPlayerPos(playerid, x, y, z);
		GetPlayerFacingAngle(playerid, a);
		
		new vehid = CreateVehicle(model, x, y, z, a, 1, 1, -1);
		PutPlayerInVehicle(playerid, vehid, 0);
		
		new dbid = gettime() + random(1000);
		VehicleDB_ID[vehid] = dbid;
		
		// Insert using End-Game MongoBuilder
		new b = MongoBuilder_Create();
		MongoBuilder_AddInt(b, "v_id", dbid);
		MongoBuilder_AddInt(b, "model", model);
		MongoBuilder_AddFloat(b, "x", x);
		MongoBuilder_AddFloat(b, "y", y);
		MongoBuilder_AddFloat(b, "z", z);
		MongoBuilder_AddFloat(b, "a", a);
		Mongo_InsertBuilder("vehicles", b); // This automatically destroys the builder
		
		SendClientMessage(playerid, -1, "Vehicle created and saved to MongoDB using BSON Builder!");
		return 1;
	}
	
	if(strcmp(cmdtext, "/vpark", true) == 0)
	{
		if(!IsPlayerInAnyVehicle(playerid)) return SendClientMessage(playerid, -1, "You must be in a vehicle.");
		new vehid = GetPlayerVehicleID(playerid);
		new dbid = VehicleDB_ID[vehid];
		if(dbid == 0) return SendClientMessage(playerid, -1, "This vehicle is not saved in the database.");
		
		new Float:x, Float:y, Float:z, Float:a;
		GetVehiclePos(vehid, x, y, z);
		GetVehicleZAngle(vehid, a);
		
		new filter[64], update[256];
		format(filter, sizeof(filter), "{\"v_id\": %d}", dbid);
		format(update, sizeof(update), "{\"$set\": {\"x\": %f, \"y\": %f, \"z\": %f, \"a\": %f}}", x, y, z, a);
		
		Mongo_UpdateOne("vehicles", filter, update);
		SendClientMessage(playerid, -1, "Vehicle parked successfully on MongoDB!");
		return 1;
	}

	return 0;
}

forward OnUserListFetch(playerid);
public OnUserListFetch(playerid)
{
	new rows = MongoCache_GetRowCount();
	if(rows > 0)
	{
		new dialog_str[2048];
		new name[MAX_PLAYER_NAME];
		for(new i = 0; i < rows; i++)
		{
			MongoCache_GetString(i, "name", name, sizeof(name));
			format(dialog_str, sizeof(dialog_str), "%s%s\n", dialog_str, name);
		}
		
		ShowPlayerDialog(playerid, 1002, DIALOG_STYLE_MSGBOX, "Registered Users List", dialog_str, "Close", "");
	}
	else
	{
		SendClientMessage(playerid, -1, "No users found in the database!");
	}
	return 1;
}

forward OnVehiclesLoad();
public OnVehiclesLoad()
{
	new rows = MongoCache_GetRowCount();
	if(rows > 0)
	{
		for(new i = 0; i < rows; i++)
		{
			new dbid = MongoCache_GetInt(i, "v_id");
			new model = MongoCache_GetInt(i, "model");
			new Float:x = MongoCache_GetFloat(i, "x");
			new Float:y = MongoCache_GetFloat(i, "y");
			new Float:z = MongoCache_GetFloat(i, "z");
			new Float:a = MongoCache_GetFloat(i, "a");
			
			new vehid = CreateVehicle(model, x, y, z, a, 1, 1, -1);
			VehicleDB_ID[vehid] = dbid;
		}
		printf("Loaded %d vehicles from MongoDB!", rows);
	}
	return 1;
}
