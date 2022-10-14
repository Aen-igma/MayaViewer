#pragma once
#include"CustomPrint.h"
#include <Windows.h>
#include <iostream>
#include"Def.h"


struct ControlHeader {
	size_t head = 0ull;
	size_t tail = 0ull;
	size_t freeMemory = 0ull;
};


class Memory {
	public:
	Memory(LPCWSTR bufferName, LPCWSTR ctrlBName, size_t bufferSize);
	~Memory();
	
	char* GetMemoryBuffer() {return mp_memoryData;}
	size_t* GetControlBuffer() {return mp_controlData;}

	size_t GetControlBufferSize() {return m_controlbufferSize;}
	size_t GetBufferSize() {return m_bufferSize;}

	private:
	HANDLE m_memoryFilemap;
	HANDLE m_controlFilemap;

	char* mp_memoryData;
	size_t* mp_controlData;

	size_t m_bufferSize;
	size_t m_controlbufferSize;

	LPCWSTR m_bufferName;
	LPCWSTR m_ctrlbufferName;

};
