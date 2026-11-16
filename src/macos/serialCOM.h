/*
 * serialCOM.h - macOS serial port access for FreeSSM
 *
 * Same interface as src/linux/serialCOM.h, implemented with POSIX termios.
 * Non-standard baud rates (e.g. 1953 baud for SSM1) are set with the
 * macOS IOSSIOSPEED ioctl, which has to be re-applied after every
 * tcsetattr() because tcsetattr() resets the speed.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SERIALCOM_H
#define SERIALCOM_H

#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
extern "C"
{
	#include <termios.h>
	#include <fcntl.h>
	#include <dirent.h>
	#include <sys/ioctl.h>
	#include <sys/select.h>
	#include <limits.h>
	#include <unistd.h>
}


class serialCOM
{

public:
	serialCOM();
	~serialCOM();
	static std::vector<std::string> GetAvailablePorts();
	bool IsOpen();
	std::string GetPortname();
	bool GetPortSettings(double *baudrate, unsigned short *databits = NULL, char *parity = NULL, float *stopbits = NULL);
	bool SetPortSettings(double baudrate, unsigned short databits, char parity, float stopbits);
	bool OpenPort(std::string portname);	// returns success of operation, NOT PORT STATUS (open/closed)
	bool ClosePort();			// returns success of operation, NOT PORT STATUS (open/closed)
	bool Write(std::vector<char> data);
	bool Write(char *data, unsigned int datalen);
	bool Read(unsigned int minbytes, unsigned int maxbytes, unsigned int timeout, std::vector<char> *data);
	bool Read(unsigned int minbytes, unsigned int maxbytes, unsigned int timeout, char *data, unsigned int *nrofbytesread);
	bool ClearSendBuffer();
	bool ClearReceiveBuffer();
	bool SendBreak(unsigned int duration_ms);
	bool SetBreak();
	bool ClearBreak();
	bool BreakIsSet();
	bool GetNrOfBytesAvailable(unsigned int *nbytes);
	bool SetControlLines(bool DTR, bool RTS);

private:
	bool applySpeed(double baudrate);

	int fd;
	bool portisopen;
	bool breakset;
	std::string currentportname;
	struct termios oldtio;		// backup of port settings
	bool settingssaved;
	double currentbaudrate;		// last rate set (termios can't report IOSSIOSPEED rates)
};


#endif
