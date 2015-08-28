#pragma once
#include "NetVar.h"
#include <thread>
#include <mutex>
#include <map>

const int MASTER_PUB_PORT = 5556;
const int MASTER_REP_PORT = 5557;
const VAR_ID MASTER_LOW_ID = 20;

class NetVar;
class MasterVar;
class SMaster;

SMaster& Master();

class SMaster
{
	enum class Type { MASTER, CLIENT, NONE };
public:
	void push_update_req(NetVar* var);
	void push_update_rep();

	void start();
	void connect(const std::string& remote_address);

	void send(const char* message);
	void subscribe(NetVar *var);

	unsigned int get_new_var_id();
	bool register_variable_req(NetVar *var);

private:
	void register_variable_rep();
	void push_var_update_sub();

	void add_var(NetVar* var);
	void rem_var(NetVar* var);

	friend class NetVar;

	struct
	{
		void *context;
		void *rep_socket;
		void *pub_socket;
		std::thread *rep_thread;

		//TODO: change the initial value to the highest enum of server commands, maybe?
		VAR_ID var_id = MASTER_LOW_ID;
		std::map<VAR_ID, MasterVar*> vars;
		std::map<std::string, VAR_ID> id_map;

	} master;

	struct
	{
		void *context;
		void *req_socket;
		void *sub_socket;
		std::string master_address;
		std::thread *sub_thread;

		std::map<VAR_ID, NetVar*> vars;

		std::mutex req_mutex;		
	} client;

	VAR_ID get_remote_new_var_id();
	VAR_ID get_var_id_req(NetVar *var);
	void get_var_id_rep();

	Type type = Type::NONE;
	std::mutex call_mutex;

// Singleton stuff
private:
	friend SMaster& Master();
	SMaster();
	~SMaster();
	SMaster(const SMaster&) = delete;
};

