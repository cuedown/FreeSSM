/*
 * j2534list - print the J2534 libraries FreeSSM can see
 *
 * Uses the same lookup code as FreeSSM (Windows registry / J2534libs.xml),
 * so if your interface is missing here, FreeSSM won't find it either.
 * The 32-bit and 64-bit builds see different registry views: most J2534
 * drivers (e.g. Tactrix OpenPort 2.0) only register a 32-bit DLL.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <iostream>
#include "J2534_API.h"
#include "J2534misc.h"

int main()
{
	std::cout << "j2534list (" << (sizeof(void*) * 8) << "-bit build)\n";
	const std::vector<J2534Library> libs = J2534_API::getAvailableJ2534Libs();
	if (libs.empty())
	{
		std::cout << "No J2534 libraries found.\n";
		if (sizeof(void*) == 8)
			std::cout << "Many J2534 drivers are 32-bit only: try the 32-bit build.\n";
		return 1;
	}
	for (const J2534Library& lib : libs)
		std::cout << " - " << lib.name << "\n   " << lib.path << "\n";
	std::cout << libs.size() << " librar" << (libs.size() == 1 ? "y" : "ies") << " found.\n";
	return 0;
}
