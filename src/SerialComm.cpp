#include "SerialComm.h"

SerialComm::SerialComm() : m_Open(false), m_Handle(nullptr)
{

}

SerialComm::~SerialComm()
{
    if (m_Open)
    {
        Close();
    }
}

Result SerialComm::Open(const std::string* device, uint32_t baud_rate)
{
    HANDLE port = CreateFileA(device->c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    delete device;
    if (port == INVALID_HANDLE_VALUE)
    {
        //print_error(device);
        //return INVALID_HANDLE_VALUE;
        return RESULT_ERROR;
    }

    BOOL success = FlushFileBuffers(port);
    if (!success)
    {
        //print_error("Failed to flush serial port");
        CloseHandle(port);
        //return INVALID_HANDLE_VALUE;
        return RESULT_ERROR;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    success = SetCommTimeouts(port, &timeouts);
    if (!success)
    {
        //print_error("Failed to set serial timeouts");
        CloseHandle(port);
        //return INVALID_HANDLE_VALUE;
        return RESULT_ERROR;
    }
    DCB state = { 0 };
    state.DCBlength = sizeof(DCB);
    state.BaudRate = baud_rate;
    state.ByteSize = 8;
    state.Parity = NOPARITY;
    state.StopBits = ONESTOPBIT;
    success = SetCommState(port, &state);
    if (!success)
    {
        //print_error("Failed to set serial settings");
        CloseHandle(port);
        //return INVALID_HANDLE_VALUE;
        return RESULT_ERROR;
    }

    m_Handle = port;
    return RESULT_SUCCESS;
}

Result SerialComm::Close()
{
    CloseHandle(m_Handle);
    m_Handle = nullptr;
    m_Open = false;

    return RESULT_SUCCESS;
}

Result SerialComm::Write(uint8_t* buffer, size_t size)
{
    DWORD written;
    BOOL success = WriteFile(m_Handle, buffer, size, &written, NULL);
    if (!success)
    {
        //print_error("Failed to write to port");
        return RESULT_ERROR;
    }
    if (written != size)
    {
        //print_error("Failed to write all bytes to port");
        return RESULT_ERROR;
    }
    return RESULT_SUCCESS;
}

Result SerialComm::Read(uint8_t* buffer, size_t size)
{
    DWORD received;
    BOOL success = ReadFile(buffer, buffer, size, &received, NULL);
    if (!success)
    {
        //print_error("Failed to read from port");
        return RESULT_ERROR;
    }
    return received;
}

Result SerialComm::WriteInt(int v)
{
    return Write((uint8_t*)&v, sizeof(v));
}

Result SerialComm::WriteFloat(float v)
{
    return Write((uint8_t*)&v, sizeof(v));
}

Result SerialComm::WriteBool(bool v)
{
    return Write((uint8_t*)&v, sizeof(v));
}

Result SerialComm::WriteByte(uint8_t v)
{
    return Write((uint8_t*)&v, sizeof(v));
}

Result SerialComm::WriteDouble(double v)
{
    return Write((uint8_t*)&v, sizeof(v));
}

Result SerialComm::WriteString(const std::string& v)
{
    return Write((uint8_t*)v.c_str(), strlen(v.c_str()));
}
