 /*  This file is part of pas.

    pas is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    pas is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pas.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <omp.h>
#include <sqlite3.h>
#include <assert.h>

#if !defined(LOG)
#if defined(_DEBUG_)
#define LOG(m)  { cerr << __FILE__ << " " << __LINE__ << " " << m << endl; }
#endif
#endif

struct Track
{
	std::string name;
};

class DB
{
public:
	DB();
	~DB();
	bool Initialize(std::string dbname);
	bool Initialized();
	int GetTrackCount();
	bool AddTrack(const Track & t);

private:
	sqlite3 * db;
	
	static int _cb_GetTrackCount(void * rv, int argc, char * argv[], char * cols[]);
};
