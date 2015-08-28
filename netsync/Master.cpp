#include "stdafx.h"
#include "Master.h"
#include <zmq.h>

class MasterVar
{
public:
	MasterVar(size_t data_size): size(data_size)
	{
		data = (byte*)malloc(data_size);
		assert(data);
	}

	~MasterVar()
	{
		free(data);
	}

	void set(byte *in)
	{
		memcpy_s(data, size, in, size);
	}

	void get(byte *out) const
	{
		memcpy_s(out, size, data, size);
	}

	const byte* const_get()
	{
		return data;
	}

private:
	byte* data;
	size_t size;

	friend class SMaster;
};

enum MasterReq : int
{
	GET_NEW_VAR_ID,			// request a new unique id from the master
	REGISTER_VARIABLE,		// send a unique id and the variable name
	SUBSCRIBE_VARIABLE,		// instruct the master to send updates for a certain id
	GET_VAR_ID,				// send a variable name and request its id
	
	// set a variable by id to a new value on the master, which will send it
	// to all clients subscribing to it.
	PUSH_VARIABLE_UDPATE,
	REQUEST_NUM
};

static int getReqFromMessage(byte *msg)
{
	return *((int*)msg);
}

static void setReqOnMessage(byte *msg, MasterReq type)
{
	*((int*)msg) = type;
}

static void setMsgFilter(byte *msg, int filter)
{

}

static byte *
s_recv(void *socket) {
	char buffer[256];
	int size = zmq_recv(socket, buffer, 255, 0);
	if (size == -1)
		return nullptr;
	if (size > 255)
		size = 255;
	buffer[size] = 0;
	return (byte*)_strdup(buffer);
}

static int
s_send(void *socket, const char *string) {
	int size = zmq_send(socket, string, strlen(string), 0);
	return size;
}

void SMaster::push_update_req(NetVar * var)
{
	std::lock_guard<std::mutex> lock(call_mutex);
	byte buffer[128];
	if (type == Type::CLIENT)
	{
		// Send message type
		setReqOnMessage(buffer, MasterReq::PUSH_VARIABLE_UDPATE);
		zmq_send(client.req_socket, buffer, sizeof(MasterReq), 0);
		zmq_recv(client.req_socket, buffer, 1, 0);

		// Send variable id
		zmq_send(client.req_socket, &var->net_id, sizeof(VAR_ID), 0);
		zmq_recv(client.req_socket, buffer, 1, 0);

		// Send data
		zmq_send(client.req_socket, &var->data, var->size, 0);
		zmq_recv(client.req_socket, buffer, 1, 0);
	}
}

void SMaster::push_update_rep()
{
	std::lock_guard<std::mutex> lock(call_mutex);
	byte buffer[128];
	VAR_ID id = 0;

	// Receive the var ID
	zmq_recv(master.rep_socket, &id, sizeof(VAR_ID), 0);
	zmq_send(master.rep_socket, buffer, 1, 0);

	MasterVar *var = master.vars.at(id);
	// Receive the var data
	zmq_recv(master.rep_socket, var->data, var->size, 0);
	zmq_send(master.rep_socket, buffer, 1, 0);

	byte msg[128];
	
	MasterReq req = MasterReq::PUSH_VARIABLE_UDPATE;
	zmq_send(master.pub_socket, &req, sizeof(req), 0);
	zmq_send(master.pub_socket, &id, sizeof(id), 0);
	zmq_send(master.pub_socket, var->data, var->size, 0);

	printf("push_update_rep\n");
}

void SMaster::start()
{
	///TODO: check for zmq failure
	master.context = zmq_ctx_new();
	master.rep_socket = zmq_socket(master.context, ZMQ_REP);
	master.pub_socket = zmq_socket(master.context, ZMQ_PUB);

	std::stringstream rep_bind;
	rep_bind << "tcp://*:";
	rep_bind << MASTER_REP_PORT;

	std::stringstream pub_bind;
	pub_bind << "tcp://*:";
	pub_bind << MASTER_PUB_PORT;

	zmq_bind(master.rep_socket, rep_bind.str().c_str());
	zmq_bind(master.pub_socket, pub_bind.str().c_str());

	type = Type::MASTER;

	master.rep_thread = new std::thread([&]()
	{
		while (1)
		{
			byte buffer[255] = {};
		//	printf("[MASTER] waiting for request ... \n");
			int size = zmq_recv(master.rep_socket, buffer, 4, 0);
			if (size == sizeof(MasterReq))
			{
				int request_type = getReqFromMessage(buffer);
				switch (request_type)
				{
				case MasterReq::GET_NEW_VAR_ID:
					printf("[MASTER] GET_NEW_VAR_ID\n");
					*((VAR_ID*)buffer) = master.var_id++;
					zmq_send(master.rep_socket, buffer, sizeof(VAR_ID), 0);
					break;
				case MasterReq::REGISTER_VARIABLE:
					zmq_send(master.rep_socket, buffer, 1, 0);
					register_variable_rep();
					break;
				case MasterReq::GET_VAR_ID:
					zmq_send(master.rep_socket, buffer, 1, 0);
					get_var_id_rep();
					break;
				case MasterReq::PUSH_VARIABLE_UDPATE:
					zmq_send(master.rep_socket, buffer, 1, 0);
					push_update_rep();
					break;
				default:
					break;
				}
			}
			else
			{
				printf("[MASTER] received null request\n");
			}
		}
	});
}

void SMaster::register_variable_rep()
{
	int size = 0;
	char buffer[255] = {};
	VAR_ID id = 0;
	// receive ID and variable name
	/*
	size = zmq_recv(master.rep_socket, buffer, sizeof(VAR_ID), 0);
	VAR_ID recv_id = *(VAR_ID*)buffer;
	printf("[MASTER] recv var id: %d\n", recv_id);
	zmq_send(master.rep_socket, buffer, 1, 0);
	*/

	size = zmq_recv(master.rep_socket, buffer, sizeof(size_t), 0);
	size_t name_length = *(size_t*)buffer;
	//printf("[MASTER] recv name size: %d\n", name_length);
	zmq_send(master.rep_socket, buffer, 1, 0);

	memset(buffer, 0, sizeof(buffer));
	size = zmq_recv(master.rep_socket, buffer, name_length, 0);
	std::string var_name(buffer);
	
	//printf("[MASTER] recv name: %s\n", var_name.c_str());

	auto it = master.id_map.find(var_name);
	if (it != master.id_map.end())
	{
		id = it->second;
		//printf("found [%s] with id [%d]\n", var_name.c_str(), id);
	}
	else
	{
		id = master.var_id++;
		master.id_map[var_name] = id;
		//printf("added [%s] with id [%d]\n", var_name.c_str(), id);
	}

	zmq_send(master.rep_socket, &id, sizeof(VAR_ID), 0);

	// receive the variable data size
	size_t data_size = 0;
	
	size = zmq_recv(master.rep_socket, &data_size, sizeof(size_t), 0);
	master.vars[id] = new MasterVar(data_size);

	//printf("registered variable: %s\n", var_name.c_str());
	zmq_send(master.rep_socket, buffer, 1, 0);
}

void SMaster::connect(const std::string & remote_address)
{
	///TODO: check for zmq failure
	int rc = 0;
	client.master_address = std::string(remote_address);
	client.context = zmq_ctx_new();

	client.req_socket = zmq_socket(client.context, ZMQ_REQ);
	client.sub_socket = zmq_socket(client.context, ZMQ_SUB);

	std::stringstream rep_bind;
	rep_bind << "tcp://";
	rep_bind << remote_address;
	rep_bind << ":";
	rep_bind << MASTER_REP_PORT;

	std::stringstream pub_bind;
	pub_bind << "tcp://";
	pub_bind << remote_address;
	pub_bind << ":";
	pub_bind << MASTER_PUB_PORT;

	zmq_connect(client.req_socket, rep_bind.str().c_str());
	zmq_connect(client.sub_socket, pub_bind.str().c_str());

	type = Type::CLIENT;

	client.sub_thread = new std::thread([&]()
	{
		while (true)
		{
			byte buffer[255] = {};
			zmq_recv(client.sub_socket, buffer, sizeof(MasterReq), 0);
			int request = getReqFromMessage(buffer);
			switch (request)
			{
			case MasterReq::PUSH_VARIABLE_UDPATE:
				push_var_update_sub();
				break;
			default:
				printf("[sub_thread] received request: %d\n", request);
			}
		}
	});
}

void SMaster::push_var_update_sub()
{
	printf("received push_variable_update\n");
	VAR_ID id = 0;
	NetVar *var = nullptr;

	zmq_recv(client.sub_socket, &id, sizeof(id), 0);
	var = client.vars.at(id);

	assert(var);

	//byte *data = (byte*) malloc(var->size);
	//zmq_recv(client.sub_socket, data, var->size, 0);

	//var->set(data);

	zmq_recv(client.sub_socket, var->data, var->size, 0);
	var->on_update_callback(*var);
}

VAR_ID SMaster::get_remote_new_var_id()
{
	std::lock_guard<std::mutex> lock(client.req_mutex);
	int rc = 0;
	char buffer[128] = {};
	*((int*)buffer) = (int)MasterReq::GET_NEW_VAR_ID;

	printf("[CLIENT] sending remote id request ... \n");
	zmq_send(client.req_socket, buffer, sizeof(MasterReq), 0);

	int size = zmq_recv(client.req_socket, buffer, sizeof(VAR_ID), 0);
	VAR_ID new_id = 0;
	if (size == sizeof(VAR_ID))
	{
		new_id = *((VAR_ID*)buffer);
		printf("[CLIENT] OK\n");
	}
	else
	{
		printf("[CLIENT] FAILED\n");
	}
	return new_id;
}

VAR_ID SMaster::get_var_id_req(NetVar *var)
{
	std::lock_guard<std::mutex> lock(client.req_mutex);
	VAR_ID id = 0;
	if (type == Type::CLIENT)
	{
		byte buffer[128] = {};
		// Send message type
		setReqOnMessage(buffer, MasterReq::GET_VAR_ID);
		zmq_send(client.req_socket, buffer, sizeof(MasterReq), 0);
		zmq_recv(client.req_socket, buffer, 1, 0);

		// Send data size
		zmq_send(client.req_socket, &var->size, sizeof(size_t), 0);
		zmq_recv(client.req_socket, buffer, 1, 0);

		// Send name length
		size_t name_length = var->name.length();
		zmq_send(client.req_socket, &name_length, sizeof(size_t), 0);
		zmq_recv(client.req_socket, &name_length, 1, 0);

		// Send name
		memcpy(buffer, var->name.c_str(), var->name.length());
		zmq_send(client.req_socket, buffer, var->name.length(), 0);
		zmq_recv(client.req_socket, &id, sizeof(VAR_ID), 0);
	}
	else if (type == Type::MASTER)
	{
		auto it = master.id_map.find(var->name);
		if (it != master.id_map.end())
		{
			id = it->second;
		}
		else
		{
			id = master.var_id++;
			master.id_map[var->name] = id;
			master.vars[id] = new MasterVar(var->size);
		}
	}
	return id;
}

void SMaster::get_var_id_rep()
{
	std::lock_guard<std::mutex> lock(client.req_mutex);
	char buffer[128] = {};
	size_t name_length = 0;
	size_t data_size = 0;

	zmq_recv(master.rep_socket, &data_size, sizeof(size_t), 0);
	zmq_send(master.rep_socket, buffer, 1, 0);

	zmq_recv(master.rep_socket, &name_length, sizeof(size_t), 0);
	zmq_send(master.rep_socket, &name_length, 1, 0);
	zmq_recv(master.rep_socket, buffer, name_length, 0);

	std::string name(buffer);

	VAR_ID id = 0;

	auto it = master.id_map.find(name);
	if (it != master.id_map.end())
	{
		id = it->second;
	}
	else
	{
		id = master.var_id++;
		master.id_map[name] = id;
		master.vars[id] = new MasterVar(data_size);
	}

	zmq_send(master.rep_socket, &id, sizeof(VAR_ID), 0);
}

bool SMaster::register_variable_req(NetVar *var)
{
	std::lock_guard<std::mutex> lock(call_mutex);
	//printf("var [%s] registering ... ", var->name.c_str());
	
	if (type == Type::CLIENT)
	{
		byte buffer[255] = {};
		// Send request type
		//zmq_recv(client.req_socket, buffer, 1, 0);
		setReqOnMessage(buffer, MasterReq::REGISTER_VARIABLE);
		zmq_send(client.req_socket, buffer, sizeof(MasterReq), 0);
		zmq_recv(client.req_socket, buffer, 1, 0);

		/*
		// Send ID
		(*(VAR_ID*)(buffer)) = var->net_id;
		zmq_send(client.req_socket, buffer, sizeof(VAR_ID), 0);
		zmq_recv(client.req_socket, buffer, 1, 0);
		*/

		// Send name length
		*(size_t*)buffer = var->name.length();
		//printf("sending name length: %d\n", var->name.length());
		zmq_send(client.req_socket, buffer, sizeof(size_t), 0);
		zmq_recv(client.req_socket, buffer, 1, 0);

		// Send name
		//printf("sending name: %s\n", var->name.c_str());
		memset(buffer, 0, sizeof(buffer));
		memcpy(buffer, var->name.c_str(), var->name.length());
		zmq_send(client.req_socket, buffer, var->name.length(), 0);

		// Receive back the id
		zmq_recv(client.req_socket, &var->net_id, sizeof(VAR_ID), 0);
		//printf("recevied ID: %u\n", var->net_id);

		// Send variable size
		//printf("sending variable size: %u\n", var->size);
		zmq_send(client.req_socket, &var->size, sizeof(size_t), 0);
		zmq_recv(client.req_socket, buffer, 1, 0);
	}
	else
	{
		auto it = master.id_map.find(var->name);
		if (it != master.id_map.end())
		{
			var->net_id = it->second;
		}
		else
		{
			var->net_id = master.var_id++;
			master.id_map[var->name] = var->net_id;
			master.vars[var->net_id] = new MasterVar(var->size);
		}
	}
	//printf("DONE\n");
	return true;
}

SMaster::SMaster()
{
	master.var_id = 20;
}

void SMaster::send(const char * message)
{
	s_send(master.pub_socket, message);
}

void SMaster::subscribe(NetVar *var)
{
	std::lock_guard<std::mutex> lock(call_mutex);
	VAR_ID id = get_var_id_req(var);
	var->net_id = id;
	//printf("subscribed to [%u]\n", id);

	if (type == Type::CLIENT || type == Type::MASTER)
	{
		zmq_setsockopt(client.sub_socket, ZMQ_SUBSCRIBE, &id, sizeof(id));
		printf("sockopt added id [%d]\n", id);
	}
}

unsigned int SMaster::get_new_var_id()
{
	std::lock_guard<std::mutex> lock(call_mutex);

	unsigned int id = 0;
	switch (type)
	{
	case SMaster::Type::MASTER:
		id = master.var_id++;
		break;
	case SMaster::Type::CLIENT:
		id = get_remote_new_var_id();
		break;
	case SMaster::Type::NONE:
		break;
	default:
		break;
	}

	return id;
}


SMaster::~SMaster()
{
	switch (type)
	{
	case SMaster::Type::MASTER:
		zmq_close(master.pub_socket);
		zmq_close(master.rep_socket);
		zmq_ctx_destroy(master.context);
		break;
	case SMaster::Type::CLIENT:
		zmq_close(client.req_socket);
		zmq_close(client.sub_socket);
		zmq_ctx_destroy(client.context);
		break;
	case SMaster::Type::NONE:
		break;
	default:
		break;
	}
}

void SMaster::add_var(NetVar *var)
{
	std::lock_guard<std::mutex> lock(call_mutex);
	assert(var != nullptr);
	client.vars[var->net_id] = var;
}

void SMaster::rem_var(NetVar *var)
{
	std::lock_guard<std::mutex> lock(call_mutex);
	assert(var != nullptr);
	client.vars.erase(var->net_id);
}

SMaster& Master()
{
	static SMaster instance;
	return instance;
}