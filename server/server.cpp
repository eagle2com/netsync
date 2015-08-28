#include "stdafx.h"
#include "../netsync/Master.h"
#include <thread>
#include <chrono>

struct v2
{
	float x;
	float y;
};

int _tmain(int argc, _TCHAR* argv[])
{
	Master().start();

	NetVar pos1("/robot1/pos1", v2{ 0,0 }, Owner::REMOTE);
	//NetVar cpuusage("/robot1/cpu", int(0), Owner::LOCAL);
	//NetVar pos1("/robot1/pos1", v2{ 0,0 }, Owner::REMOTE);
	//NetVar pos2("/robot1/pos2", v2{ 0,0 }, Owner::LOCAL);

	
	pos1.onUpdate([](NetVar& pos)
	{
		v2 p = pos.get<v2>();
		printf("pos1.onUpdate()\n");
		// be awesome
	});
	/*
	// Lets send some updates.

	for (int i = 0; i < 20; i++)
	{
		pos1.set(v2{ (float)i,(float)i*2 });
		cpuusage.set<int>(i);
		// pos2.set(v2{ (float)i,(float)i * 2 }); <---- THIS WILL NOT WORK, the variable is remote
		// you do not have ownership of it.
	}*/
	
	

	getchar();
	return 0;
}

