#include "NamedPipeClientWindows.h"


NamedPipeClientWindows::NamedPipeClientWindows(LPTSTR name):_pipeName(name)
{
	
}

NamedPipeClientWindows::~NamedPipeClientWindows()
{

}

void NamedPipeClientWindows::send(unsigned char * msg, size_t size)
{
	BOOL success = WriteFile(
		_pipe,                  // pipe handle 
		msg,             // message 
		size,              // message length 
		&_cbWritten,             // bytes written 
		NULL); // not overlapped 

	if (!success)
	{
		sendError("Unabled to send message", -3);
	}
}

void NamedPipeClientWindows::close()
{
	_reading = false;
	CloseHandle(_pipe);
}

//This will stall
bool NamedPipeClientWindows::open()
{

	_pipe = CreateFile(
	_pipeName,   // pipe name 
	GENERIC_READ |  // read and write access 
	GENERIC_WRITE,
	0,              // no sharing 
	NULL,           // default security attributes
	OPEN_EXISTING,  // opens existing pipe 
	0,              // default attributes 
	NULL);          // no template file 

	// Break if the pipe handle is valid. 
	if (_pipe == INVALID_HANDLE_VALUE)
	{
		sendError( "Invalid Handle Value", 0);
	}
	else
	{
		printf("Pipe Connected: %s \n", _pipeName);
		if (setPipeState(PIPE_READMODE_BYTE))
		{
			startReadPolling();
			return true;
		}
		else
		{ 
			sendError("Failed to set PIPE_READMODE", -2);
			return false;
		}
	}
		sendError("Insert unhelpful error message here", GetLastError());

	// All pipe instances are busy, so wait for 20 seconds. 

	if (!WaitNamedPipe(_pipeName, 10000))
	{
		sendError("Could not open pipe: 10 second wait timed out.", -1);
	}

	return false;
}

void NamedPipeClientWindows::onRecieve(void(*returnFunct)(unsigned char *, size_t size))
{
	_onRecieveCallBack = returnFunct;
}

void NamedPipeClientWindows::errorCallback(void(*errorFunct)(std::string msg, unsigned int ec))
{
	_onErrorCallback = errorFunct;
}

bool NamedPipeClientWindows::setPipeState(DWORD state)
{
	return SetNamedPipeHandleState(
		_pipe,    // pipe handle 
		&state,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 
}

void NamedPipeClientWindows::sendError(std::string msg, unsigned int ec)
{
	if (_onErrorCallback)
		_onErrorCallback(msg,ec);
}

void NamedPipeClientWindows::sendRecieve(unsigned char * data, size_t size)
{
	if (_onRecieveCallBack)
	{
		_onRecieveCallBack(data, size);
	}
	else
	{
		sendError("Recieve Callback not set. \n", -5);
	}
}

void NamedPipeClientWindows::startReadPolling()
{
	_reading = true;
	std::thread poll_thread(&NamedPipeClientWindows::pollServer, this);
	poll_thread.detach();
}

void NamedPipeClientWindows::pollServer()
{
	printf("Started polling %s server. \n", _pipeName);
		while (_reading) {
			BOOL success = FALSE;
			while (!success) {

			success = ReadFile(
				_pipe,    // pipe handle 
				_readBuffer,    // buffer to receive reply 
				BUF_SIZE * sizeof(unsigned char),  // size of buffer 
				&_cbRead,  // number of bytes read 
				NULL);    // not overlapped 

			if (!success && GetLastError() != ERROR_MORE_DATA)
				sendError("Failed on read. Stopping Read.", GetLastError());
			else if (GetLastError() == ERROR_MORE_DATA)
				sendError("More data on the way!", GetLastError());
		}
		sendRecieve(_readBuffer, _cbRead);
	}
}
