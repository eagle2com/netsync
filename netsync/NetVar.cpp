#include "stdafx.h"
#include "NetVar.h"

NetVar::~NetVar()
{
	Master().rem_var(this);
	delete data;
}
	
std::mutex NetVar::ctor_mutex;