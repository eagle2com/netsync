#pragma once

using VAR_ID = unsigned int;

#include "Master.h"
#include <string>
#include <functional>
#include <iostream>
#include <assert.h>
#include <thread>
#include <mutex>
#include <atomic>

enum class Owner {LOCAL, REMOTE};


class NetVar
{
public:
	template<typename T>
	NetVar(const std::string& name, T value, Owner owner):
		name(name), size(sizeof(T)), owner(owner)
	{
		
		data = new char[size];
		memcpy(data, &value, size);
		network_ready = false;
		
		register_thread = new std::thread([&]() 
		{
			//std::cout << "ctor: " << (int)this->owner << std::endl;
			if (this->owner == Owner::LOCAL)
			{
				//std::cout << name << ": localy owned, registering" << std::endl;
				Master().register_variable_req(this);
				printf("registered [%s] with id [%d]\n", this->name.c_str(), net_id);
			}
			else if(this->owner == Owner::REMOTE)
			{
				Master().subscribe(this);
				printf("subscribed [%s] with id [%d]\n", this->name.c_str(), net_id);
			}
			network_ready = true;
			printf("network ready\n");
			Master().add_var(this);
			Master().push_update_req(this);
		});	
	}

	~NetVar();

	const size_t size;
	const std::string name;
	

	template<typename T>
	T get() const
	{
		return *((T*)data);
	}

	template<typename T>
	void get(T& out) const
	{
		out = *((T*)data);
	}

	template<typename T>
	void set(const T& value)
	{
		assert(sizeof(T) == size);
		assert(owner == Owner::LOCAL);
		memcpy(data, &value, size);
		on_update_callback(*this);
		if (network_ready)
		{
			printf("asking Master for update push\n");
			Master().push_update_req(this);
		}
	}

	
	
	/// <summary>
	/// Set the callback to be executed whenever the variable receives an update from the master.
	/// </summary>
	/// <param name="callback">The callback</param>
	void onUpdate(std::function<void(NetVar&)> callback)
	{
		on_update_callback = callback;
	}

	void setPushFrenquency(unsigned int f)
	{
		push_frequency = f;
	}

	int getPushFrequency() const
	{
		return push_frequency;
	}

	const Owner owner;

private:
	void set(const void* value)
	{
		memcpy(data, value, size);
		on_update_callback(*this);
	}



	void *data;
	std::function<void(NetVar&)> on_update_callback = [](NetVar&) {};
	VAR_ID net_id;
	std::thread *register_thread;
	std::atomic_bool network_ready;

	unsigned int push_frequency = 1;

	static std::mutex ctor_mutex;

	friend class SMaster;
};
