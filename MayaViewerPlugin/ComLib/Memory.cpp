#include "Memory.h"

Memory::Memory(LPCWSTR bufferName, LPCWSTR ctrlBName, size_t bufferSize)
	:m_memoryFilemap(), m_controlFilemap(), mp_memoryData(nullptr), mp_controlData(nullptr),
	m_bufferSize(bufferSize), m_controlbufferSize(sizeof(ControlHeader)), m_bufferName(bufferName), m_ctrlbufferName(ctrlBName) {

	// Main File map
	m_memoryFilemap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0ul, (DWORD)bufferSize, bufferName);
	if(GetLastError() == ERROR_ALREADY_EXISTS) Print("File Mapping is linked.\n");

	if(!m_memoryFilemap) 
		Print("ERROR: Failed to create File Mapping.\n");
	else 
		mp_memoryData = (char*)MapViewOfFile(m_memoryFilemap, FILE_MAP_ALL_ACCESS, 0ul, 0ul, bufferSize);

	if(!mp_memoryData) Print("ERROR: View of File Mapping failed.\n");


	// Control File Map
	m_controlFilemap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0ul, (DWORD)m_controlbufferSize, m_ctrlbufferName);
	if(GetLastError() == ERROR_ALREADY_EXISTS) Print("Control File Map is linked.\n");

	if(!m_controlFilemap)
		Print("ERROR: Failed to create Control File Mapping.\n");
	else 
		mp_controlData = (size_t*)MapViewOfFile(m_controlFilemap, FILE_MAP_ALL_ACCESS, 0ul, 0ul, m_controlbufferSize);

	if(!mp_controlData) Print("ERROR: View of File Mapping failed.\n");
}

Memory::~Memory() {
	UnmapViewOfFile(mp_memoryData);
	CloseHandle(m_memoryFilemap);

	UnmapViewOfFile(mp_controlData);
	CloseHandle(m_controlFilemap);
}
