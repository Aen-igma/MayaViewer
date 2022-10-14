#pragma once
#include"CustomPrint.h"

// Header for sending information about next message
struct MessageHeader {
	size_t messageID;
	size_t messageLength;
};