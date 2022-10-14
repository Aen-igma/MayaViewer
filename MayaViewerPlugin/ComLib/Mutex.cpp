#include "Mutex.h"


Mutex::Mutex(LPCWSTR mutexName)
	:m_mutexHandle() {

	m_mutexHandle = CreateMutex(nullptr, false, mutexName);
	if(!m_mutexHandle) Print("ERROR: Failed to create Mutex.\n");
	if(GetLastError() == ERROR_ALREADY_EXISTS) Print("Mutex linked.\n");



}

Mutex::~Mutex() {
	CloseHandle(m_mutexHandle);
}

void Mutex::Lock() {
	WaitForSingleObject(m_mutexHandle, INFINITE);
}

void Mutex::Unlock() {
	ReleaseMutex(m_mutexHandle);
}
