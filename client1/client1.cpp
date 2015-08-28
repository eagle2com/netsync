#include "stdafx.h"
#include "../netsync/Master.h"

struct v2
{
	float x;
	float y;
};

int _tmain(int argc, _TCHAR* argv[])
{
	Master().connect("localhost");

	//printf("variables getting their ids...\n");
	NetVar pos1("/robot1/pos1", v2{0,0}, Owner::LOCAL);
	getchar();
	pos1.set(v2{ 1,1 });

	//NetVar pos2("/robot1/pos2", v2{ 0,0 }, Owner::REMOTE);
	//NetVar pos1("/robot1/pos3", v2{ 0,0 }, Owner::REMOTE);

	//NetVar cpuusage("/robot1/cpu", int(0), Owner::LOCAL);
	//NetVar pos2("/robot2/pos", v2{0,0}, Owner::LOCAL);

	//printf("done\n");
	/*
	pos1.onUpdate([](NetVar& pos)
	{
		v2 p = pos.get<v2>();
		// be awesome
	});

	cpuusage.onUpdate([](NetVar& c)
	{
		int usage = c.get<int>();
		// check on his health
		printf("cpu usage: %d\n", usage);
	});*/

	// And all of this is of course thread-safe =).

	getchar();

	return 0;
}

