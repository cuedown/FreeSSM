/*
 * serialCOM.cpp - macOS serial port access for FreeSSM
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "serialCOM.h"
#include <cmath>
#include <cerrno>
#include <ctime>
#include <cstdlib>	// getenv
#include <IOKit/serial/ioss.h>	// IOSSIOSPEED


serialCOM::serialCOM()
{
	fd = -1;
	portisopen = false;
	breakset = false;
	currentportname = "";
	settingssaved = false;
	currentbaudrate = 0;
	memset(&oldtio, 0, sizeof(oldtio));
}


serialCOM::~serialCOM()
{
	if (portisopen)
		ClosePort();
}


std::vector<std::string> serialCOM::GetAvailablePorts()
{
	// macOS: use the callout devices (/dev/cu.*), which don't wait for DCD
	std::vector<std::string> portlist;
	DIR *dp = opendir("/dev");
	if (dp == NULL)
		return portlist;
	struct dirent *ep;
	while ((ep = readdir(dp)) != NULL)
	{
		if (strncmp(ep->d_name, "cu.", 3) != 0)
			continue;
		// skip ports that are never diagnostic interfaces
		if (strstr(ep->d_name, "Bluetooth-Incoming-Port") || strstr(ep->d_name, "debug-console"))
			continue;
		portlist.push_back(std::string("/dev/") + ep->d_name);
	}
	closedir(dp);
	// extra ports for testing, e.g. the pseudo-terminal of tools/fake_ecu.py:
	// FREESSM_EXTRA_PORTS=/dev/ttys005:/dev/ttys006
	const char *extra = getenv("FREESSM_EXTRA_PORTS");
	if (extra != NULL)
	{
		std::string list(extra);
		size_t start = 0;
		while (start <= list.size())
		{
			size_t end = list.find(':', start);
			if (end == std::string::npos)
				end = list.size();
			if (end > start)
				portlist.push_back(list.substr(start, end - start));
			start = end + 1;
		}
	}
	return portlist;
}


bool serialCOM::IsOpen()
{
	return portisopen;
}


std::string serialCOM::GetPortname()
{
	return currentportname;
}


bool serialCOM::applySpeed(double baudrate)
{
	speed_t speed = static_cast<speed_t>(round(baudrate));
	if (speed < 1)
		return false;
	// IOSSIOSPEED accepts any rate the driver supports (standard or not)
	if (ioctl(fd, IOSSIOSPEED, &speed) == -1)
	{
		// not a real serial driver (e.g. a pseudo-terminal): fall back to plain termios
		struct termios tio;
		if ((tcgetattr(fd, &tio) == -1) || (cfsetspeed(&tio, speed) == -1) || (tcsetattr(fd, TCSANOW, &tio) == -1))
			return false;
	}
	currentbaudrate = baudrate;
	return true;
}


bool serialCOM::GetPortSettings(double *baudrate, unsigned short *databits, char *parity, float *stopbits)
{
	if (!portisopen)
		return false;
	struct termios tio;
	if (tcgetattr(fd, &tio) == -1)
		return false;
	if (baudrate)
	{
		if (currentbaudrate > 0)
			*baudrate = currentbaudrate;
		else
			*baudrate = static_cast<double>(cfgetospeed(&tio));	// on macOS speed_t is the plain rate
	}
	if (databits)
	{
		switch (tio.c_cflag & CSIZE)
		{
			case CS5: *databits = 5; break;
			case CS6: *databits = 6; break;
			case CS7: *databits = 7; break;
			default:  *databits = 8; break;
		}
	}
	if (parity)
	{
		if (!(tio.c_cflag & PARENB))
			*parity = 'N';
		else if (tio.c_cflag & PARODD)
			*parity = 'O';
		else
			*parity = 'E';
	}
	if (stopbits)
	{
		if (tio.c_cflag & CSTOPB)
			*stopbits = ((tio.c_cflag & CSIZE) == CS5) ? 1.5f : 2.0f;
		else
			*stopbits = 1.0f;
	}
	return true;
}


bool serialCOM::SetPortSettings(double baudrate, unsigned short databits, char parity, float stopbits)
{
	if (!portisopen || (baudrate < 1))
		return false;
	struct termios newtio;
	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = CLOCAL | CREAD;
	switch (databits)
	{
		case 5: newtio.c_cflag |= CS5; break;
		case 6: newtio.c_cflag |= CS6; break;
		case 7: newtio.c_cflag |= CS7; break;
		case 8: newtio.c_cflag |= CS8; break;
		default: return false;
	}
	if (parity == 'N')
		;
	else if (parity == 'E')
		newtio.c_cflag |= PARENB;
	else if (parity == 'O')
		newtio.c_cflag |= PARENB | PARODD;
	else
		return false;	// mark/space parity (CMSPAR) is not available on macOS
	if (stopbits == 1)
		;
	else if ((stopbits == 1.5f) && (databits == 5))
		newtio.c_cflag |= CSTOPB;
	else if ((stopbits == 2) && (databits != 5))
		newtio.c_cflag |= CSTOPB;
	else
		return false;
	// raw mode, same choices as the Linux implementation:
	newtio.c_lflag = NOFLSH;
	newtio.c_iflag = IGNPAR | IGNBRK;
	newtio.c_oflag = 0;
	newtio.c_cc[VMIN] = 0;
	newtio.c_cc[VTIME] = 0;
	// tcsetattr needs a valid standard rate; the real rate is applied afterwards
	cfsetspeed(&newtio, B9600);
	if (tcsetattr(fd, TCSANOW, &newtio) == -1)
		return false;
	return applySpeed(baudrate);
}


bool serialCOM::OpenPort(std::string portname)
{
	if (portisopen)
		return false;
	fd = open(portname.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
		return false;
	portisopen = true;
	currentportname = portname;
	ioctl(fd, TIOCEXCL);	// exclusive access (failure is not fatal)
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)	// read() returns 0 if no data is available
	{
		ClosePort();
		return false;
	}
	settingssaved = (tcgetattr(fd, &oldtio) != -1);
	breakset = false;
	if (tcflush(fd, TCIOFLUSH) == -1)
	{
		ClosePort();
		return false;
	}
	if (!SetPortSettings(9600, 8, 'N', 1))
	{
		ClosePort();
		return false;
	}
	SetControlLines(true, true);	// after SetPortSettings (drivers may change DTR/RTS)
	return true;
}


bool serialCOM::ClosePort()
{
	if (!portisopen)
		return false;
	ioctl(fd, TIOCCBRK);
	breakset = false;
	tcflush(fd, TCIOFLUSH);
	if (settingssaved)
		tcsetattr(fd, TCSANOW, &oldtio);
	ioctl(fd, TIOCNXCL);
	if (close(fd) != 0)
		return false;
	fd = -1;
	portisopen = false;
	currentportname = "";
	currentbaudrate = 0;
	return true;
}


bool serialCOM::Write(std::vector<char> data)
{
	if (data.empty())
		return true;
	return Write(&data.at(0), data.size());
}


bool serialCOM::Write(char *data, unsigned int datalen)
{
	if (!portisopen)
		return false;
	if (breakset)
	{
		if (ioctl(fd, TIOCCBRK) == -1)
			return false;
		breakset = false;
	}
	unsigned int written = 0;
	while (written < datalen)
	{
		ssize_t ret = write(fd, data + written, datalen - written);
		if (ret > 0)
			written += static_cast<unsigned int>(ret);
		else if ((ret == -1) && (errno == EAGAIN))
		{
			fd_set output;
			FD_ZERO(&output);
			FD_SET(fd, &output);
			struct timeval tv = {1, 0};
			if (select(fd+1, NULL, &output, NULL, &tv) <= 0)
				return false;
		}
		else
			return false;
	}
	return (tcdrain(fd) == 0);	// wait until all bytes are transmitted
}


bool serialCOM::Read(unsigned int minbytes, unsigned int maxbytes, unsigned int timeout, std::vector<char> *data)
{
	if (!portisopen || (minbytes > maxbytes) || (maxbytes > INT_MAX))
		return false;
	unsigned int rdatalen = 0;
	char *rdata = (char*) malloc(maxbytes ? maxbytes : 1);
	if (rdata == NULL)
		return false;
	bool ok = Read(minbytes, maxbytes, timeout, rdata, &rdatalen);
	if (ok)
		data->assign(rdata, rdata+rdatalen);
	free(rdata);
	return ok;
}


static long long monotonic_ms()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return static_cast<long long>(t.tv_sec)*1000 + t.tv_nsec/1000000;
}


bool serialCOM::Read(unsigned int minbytes, unsigned int maxbytes, unsigned int timeout, char *data, unsigned int *nrofbytesread)
{
	*nrofbytesread = 0;
	if (!portisopen || (minbytes > maxbytes) || (maxbytes > INT_MAX))
		return false;
	if (!minbytes)
	{
		ssize_t ret = read(fd, data, maxbytes);
		if ((ret == -1) && (errno == EAGAIN))
			ret = 0;
		if ((ret < 0) || (ret > static_cast<ssize_t>(maxbytes)))
			return false;
		*nrofbytesread = static_cast<unsigned int>(ret);
		return true;
	}
	unsigned int rb_total = 0;
	const long long t_end = monotonic_ms() + timeout;
	while (rb_total < minbytes)
	{
		fd_set input;
		FD_ZERO(&input);
		FD_SET(fd, &input);
		int n;
		if (timeout > 0)
		{
			long long remaining = t_end - monotonic_ms();
			if (remaining <= 0)
				break;	// timeout
			struct timeval tv;
			tv.tv_sec = remaining / 1000;
			tv.tv_usec = (remaining % 1000) * 1000;
			n = select(fd+1, &input, NULL, NULL, &tv);
		}
		else
			n = select(fd+1, &input, NULL, NULL, NULL);	// wait indefinitely
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			return false;
		}
		if (n == 0)
			break;	// timeout
		ssize_t ret = read(fd, data + rb_total, maxbytes - rb_total);
		if ((ret == -1) && (errno == EAGAIN))
			continue;
		if ((ret < 0) || (ret > static_cast<ssize_t>(maxbytes - rb_total)))
			return false;
		rb_total += static_cast<unsigned int>(ret);
	}
	// like Linux: return what was received, even if fewer than minbytes (timeout)
	*nrofbytesread = rb_total;
	return true;
}


bool serialCOM::ClearSendBuffer()
{
	return portisopen && (tcflush(fd, TCOFLUSH) == 0);
}


bool serialCOM::ClearReceiveBuffer()
{
	return portisopen && (tcflush(fd, TCIFLUSH) == 0);
}


bool serialCOM::SendBreak(unsigned int duration_ms)
{
	if (!portisopen || (duration_ms < 1) || (duration_ms >= 32767))
		return false;
	if (ioctl(fd, TIOCSBRK) == -1)
		return false;
	breakset = true;
	usleep(1000 * duration_ms);
	if (ioctl(fd, TIOCCBRK) == -1)
		return false;
	breakset = false;
	return true;
}


bool serialCOM::SetBreak()
{
	if (!portisopen || (ioctl(fd, TIOCSBRK) == -1))
		return false;
	breakset = true;
	return true;
}


bool serialCOM::ClearBreak()
{
	if (!portisopen || (ioctl(fd, TIOCCBRK) == -1))
		return false;
	breakset = false;
	return true;
}


bool serialCOM::BreakIsSet()
{
	return breakset;
}


bool serialCOM::GetNrOfBytesAvailable(unsigned int *nbytes)
{
	int bytes = 0;
	if (!portisopen || (ioctl(fd, FIONREAD, &bytes) == -1))
		return false;
	*nbytes = static_cast<unsigned int>(bytes);
	return true;
}


bool serialCOM::SetControlLines(bool DTR, bool RTS)
{
	if (!portisopen)
		return false;
	int linestatus = 0;
	if (ioctl(fd, TIOCMGET, &linestatus) == -1)
		return false;
	if (DTR) linestatus |= TIOCM_DTR; else linestatus &= ~TIOCM_DTR;
	if (RTS) linestatus |= TIOCM_RTS; else linestatus &= ~TIOCM_RTS;
	return (ioctl(fd, TIOCMSET, &linestatus) != -1);
}
