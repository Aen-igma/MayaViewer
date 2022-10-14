#include "Comlib.h"

Comlib::Comlib(LPCWSTR bufferName, size_t bufferSize, ProcessType type)
    :mp_mutex(nullptr), mp_sharedMemory(nullptr), mp_messageData(nullptr), mp_head(nullptr), mp_tail(nullptr), mp_freeMemory(nullptr),
    mp_messageHeader(nullptr), mp_ctrler(nullptr), m_type(type) {

    std::wstring ctrBName(bufferName);
    ctrBName += +L"_ctrBuffer";
    mp_sharedMemory = NEW Memory(bufferName, ctrBName.c_str(), bufferSize);
    mp_messageData = mp_sharedMemory->GetMemoryBuffer();
    mp_mutex = NEW Mutex(L"MutexMap");


    mp_head = mp_sharedMemory->GetControlBuffer();
    mp_tail = mp_head + 1;
    mp_freeMemory = mp_tail + 1;

    if(type == ProcessType::Producer) {
        Print("Producer Initialized.\n");

        *mp_head = 0ull;
        *mp_tail = 0ull;
        *mp_freeMemory = bufferSize;

    } else if(type == ProcessType::Consumer) {
        Print("Consumer Initialized.\n");

        *mp_tail = 0ull;

    }
}

Comlib::~Comlib() {
    delete mp_sharedMemory;
    delete mp_mutex;
}

bool Comlib::Send(void* message, MessageHeader* msgHeader) {

    mp_mutex->Lock();

    size_t memoryLeft = mp_sharedMemory->GetBufferSize() - *mp_head;
    if(msgHeader->messageLength + sizeof(MessageHeader) >= memoryLeft) {

        if(*mp_tail != 0ull) {
            msgHeader->messageID = 0ull;
            memcpy(mp_messageData + *mp_head, msgHeader, sizeof(MessageHeader));

            *mp_freeMemory -= (msgHeader->messageLength + sizeof(MessageHeader));
            *mp_head = 0ull;

        }

    } else if(msgHeader->messageLength + sizeof(MessageHeader) < *mp_freeMemory - 1ull) {
        msgHeader->messageID = 1ull;

        memcpy(mp_messageData + *mp_head, msgHeader, sizeof(MessageHeader));
        memcpy(mp_messageData + *mp_head + sizeof(MessageHeader), message, msgHeader->messageLength);

        *mp_freeMemory -= (msgHeader->messageLength + sizeof(MessageHeader));
        *mp_head = (*mp_head + msgHeader->messageLength + sizeof(MessageHeader)) % mp_sharedMemory->GetBufferSize();

        mp_mutex->Unlock();
        return true;

    }

    mp_mutex->Unlock();
    return false;
}

bool Comlib::Recieve(void* message) {

    mp_mutex->Lock();

    size_t msgLength(0ull);
    if(*mp_freeMemory < mp_sharedMemory->GetBufferSize()) {

        if(*mp_head != *mp_tail) {
            mp_messageHeader = (MessageHeader*)&mp_messageData[*mp_tail];

            msgLength = mp_messageHeader->messageLength;
            if(mp_messageHeader->messageID == 0ull) {
                *mp_freeMemory += (msgLength + sizeof(MessageHeader));
                *mp_tail = 0ull;

            } else {
                memcpy(message, &mp_messageData[*mp_tail + sizeof(MessageHeader)], msgLength);

                *mp_freeMemory += (msgLength + sizeof(MessageHeader));
                *mp_tail = (*mp_tail + msgLength + sizeof(MessageHeader)) % mp_sharedMemory->GetBufferSize();

                mp_mutex->Unlock();
                return true;
                
            }
        }
    }

    mp_mutex->Unlock();
    return false;
}

bool Comlib::Inject(void** message) {

    mp_mutex->Lock();

    size_t msgLength(0ull);
    if(!*message) {
        if(*mp_freeMemory < mp_sharedMemory->GetBufferSize()) {

            if(*mp_head != *mp_tail) {
                mp_messageHeader = (MessageHeader*)&mp_messageData[*mp_tail];

                msgLength = mp_messageHeader->messageLength;
                if(mp_messageHeader->messageID == 0ull) {
                    *mp_freeMemory += (msgLength + sizeof(MessageHeader));
                    *mp_tail = 0ull;

                } else {
                    *message = NEW char[msgLength];
                    memcpy(*message, &mp_messageData[*mp_tail + sizeof(MessageHeader)], msgLength);

                    *mp_freeMemory += (msgLength + sizeof(MessageHeader));
                    *mp_tail = (*mp_tail + msgLength + sizeof(MessageHeader)) % mp_sharedMemory->GetBufferSize();

                    mp_mutex->Unlock();
                    return true;

                }
            }
        }
    }

    mp_mutex->Unlock();
    return false;
}

void Comlib::ClearMemory() {
    ZeroMemory(&mp_messageData, mp_sharedMemory->GetBufferSize());
}
