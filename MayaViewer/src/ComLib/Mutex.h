#pragma once
#include"CustomPrint.h"
#include <Windows.h>
#include <iostream>

class Mutex {
	public:
	Mutex(LPCWSTR mutexName);
	~Mutex();

	void Lock();
	void Unlock();

	private:
	HANDLE m_mutexHandle;

};
