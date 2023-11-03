#pragma once
#include <windows.h>
#include <stdint.h>
#include <string>

typedef unsigned char Result;

#define RESULT_SUCCESS 0
#define RESULT_ERROR 1

class SerialComm
{
private:
	bool m_Open;
	HANDLE m_Handle;
public:
	SerialComm();
	~SerialComm();

	Result Open(const std::string* device, uint32_t baud_rate);
	Result Close();
	Result Write(uint8_t* buffer, size_t size);
	Result Read(uint8_t* buffer, size_t size);

	Result WriteInt(int v);
	Result WriteFloat(float v);
	Result WriteBool(bool v);
	Result WriteByte(uint8_t v);
	Result WriteDouble(double v);
	Result WriteString(const std::string& v);
public:
	static std::string* GetDevice(const std::string& device)
	{
		std::string* str = new std::string("\\\\.\\");
		str->append(device);
		return str;
	}
};