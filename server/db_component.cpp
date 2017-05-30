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

/*	Copyright 2017 by Perr Kivolowitz
 */

#include "db_component.hpp"
#include "logger.hpp"
#include "db.hpp"

using namespace std;
using namespace pas;

extern Logger _log_;

void ReformatSQLException(sql::SQLException &e) {
	stringstream ss;
	ss << "# ERR: SQLException in " << __FILE__;
	ss << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
	ss << "# ERR: " << e.what();
	ss << " (MySQL error code: " << e.getErrorCode();
	ss << ", SQLState: " << e.getSQLState() << " )" << endl;
	throw LOG2(_log_, ss.str(), LogLevel::FATAL);
}

/*	InitPreparedStatements() Prepared statements in a DB system
	remove the possibility of SQL injection attacks by encapsulating
	all user provided data. They are rigidly controlled in terms of
	column names and order, etc.

	This function creates two strings that will be used in prepared
	statements for writing to the tracks table as well as reading from
	it. 

	For example:
		insert into tracks <query_columns> <parameter columns> ...
*/

void DB::InitPreparedStatement()
{
	query_columns = "(";
	parameter_columns = " values (";
	size_t n = sizeof(track_column_names) / sizeof(string);
	for (size_t i = 0; i < n; i++) {
		if (i > 0) {
			query_columns += ", ";
			parameter_columns += ", ";
			select_columns += ", ";
		}
		query_columns += track_column_names[i];
		select_columns += track_column_names[i];
		parameter_columns += "?";
	}
	query_columns += ") ";
	parameter_columns += ") ";
	select_columns += ", id";
}

DB::DB()
{
	driver = nullptr;
	connection = nullptr;
	InitPreparedStatement();
}

DB::~DB()
{
	if (connection != nullptr)
	{
		connection->close();
		delete connection;
	}
}

/*	Initialize() can throw LoggedExceptions now. It is called from
	audio_component and connection_manager. Check now to see the
	side effects of being able to throw exceptions now.
*/
bool DB::Initialize(string dbhost)
{
	bool rv = true;

	string dbh = "tcp://" + dbhost + ":3306";

	try {
		driver = get_driver_instance();
		if (driver == nullptr) {
			throw LOG2(_log_, "get_driver_instance() failed", LogLevel::FATAL);
			rv = false;
		}
		else {
			connection = driver->connect(dbh.c_str(), "pas", "pas");
			if (connection == nullptr) {
				throw LOG2(_log_, "connect() failed", LogLevel::FATAL);
			}
			else {
				connection->setSchema("pas2");
			}
		}
	}
	catch (sql::SQLException & e) {
		ReformatSQLException(e);
	}
	return rv;
}

void DB::DeInitialize()
{
	if (connection != nullptr)
	{
		connection->close();
		delete connection;
		connection = nullptr;
	}
}

/*	NOTE: nspace defaults to "default"
*/

int DB::GetRoot(string nspace)
{
	assert(connection != nullptr);
	LOG2(_log_, nullptr, REDICULOUS);

	if (nspace.size() == 0)
		nspace = "default";

	string sql = "select me from paths where up = -1 and namespace like " + nspace + ";";

	return IntegerQuery(sql);
}

int DB::IntegerQuery(string & sql)
{
	int rv = 0;

	sql::Statement * stmt = nullptr;
	sql::ResultSet *res = nullptr;

	assert(connection != nullptr);

	stmt = connection->createStatement();
	if (stmt == nullptr)
	{
		LOG2(_log_, "createStatement() failed", FATAL);
	}
	else
	{
		try
		{
			res = stmt->executeQuery(sql.c_str());
			if (res->next())
			{
				rv = res->getInt(1);
			}
		}
		catch (sql::SQLException &e) {
			ReformatSQLException(e);
		}
	}
	if (stmt != nullptr)
		delete stmt;

	if (res != nullptr)
		delete res;

	return rv;
}

/*	NOTE: nspace defaults to "default"
*/
int DB::GetTrackCount(string nspace)
{
	// Make this a prepared statement.
	string sql("select count(*) from tracks where namespace like \'" + nspace + "\'");
	return IntegerQuery(sql);
}

/*	NOTE: nspace defaults to "default"
*/

void DB::GetSubfolders(SelectResult & results, int id, string nspace)
{	
}

void DB::GetDeviceInfo(std::string alsa_name, std::string & friendly_name)
{
	friendly_name = "";

	string s = "select friendly_name from devices where alsa_name=?;";
	sql::PreparedStatement * stmt = connection->prepareStatement(s.c_str());
	stmt->setString(1, alsa_name);
	sql::ResultSet * res = stmt->executeQuery();
	if (res->next())
	{
		friendly_name = res->getString(1);
	}
	LOG2(_log_, alsa_name, VERBOSE);	
	LOG2(_log_, friendly_name, VERBOSE);
	if (res)
		delete res;

	if (stmt)
		delete stmt;
}

void DB::InnerGetTracks(pas::SelectResult & results, string & sql)
{
	assert(connection != nullptr);
	sql::Statement * stmt = nullptr;
	sql::ResultSet * res = nullptr;
	
	LOG2(_log_, sql, CONVERSATIONAL);

	try
	{
		if ((stmt = connection->createStatement()) == nullptr)
			throw LOG2(_log_, "createStatement() failed", FATAL);

		res = stmt->executeQuery(sql.c_str());

		while (res->next())
		{
			Row * r = results.add_row();
			r->set_type(ROW);
		
			google::protobuf::Map<string, string> * m = r->mutable_results();

			for (size_t i = 0; i < sizeof(track_column_names) / sizeof(string); i++)
			{
				string col = track_column_names[i];
				string s;
				s = res->getString(col);
				(*m)[col] = s;
			}

			(*m)["id"] = res->getString("id");
			(*m)["type"] = "track";
	
			// Some tracks have no title tag. In this
			// case, substitute in the filename.
			if ((*m)["title"].size() == 0)
				(*m)["title"] = (*m)["fname"];
		}
	}
	catch (LoggedException s)
	{
		if (s.Level() == FATAL)
			throw s;
	}
	catch (sql::SQLException &e)
	{
		// This is a throw.
		ReformatSQLException(e);
	}

	// Mem leak is a throw took place!

	if (stmt != nullptr)
		delete stmt;

	if (res != nullptr)
		delete res;

}

void DB::GetTracks(SelectResult & results, int id, string nspace)
{	
	if (nspace.size() == 0)
		nspace = "default";

	string sql;
	sql = "select " + select_columns + " from tracks where ";
	sql += "parent = " + to_string(id) + " ";
	sql += "and namespace like \"" + nspace + "\" ";
	sql += "order by track;";
	InnerGetTracks(results, sql);
}

void DB::GetFolder(SelectResult & results, int id, string nspace)
{
}


/*	NOTE: nspace defaults to "default"
	NOTE: orderby defaults to ""
*/
void DB::MultiValuedQuery(
	string column, 
	string pattern, 
	SelectResult & results, 
	string nspace,
	string orderby
)
{
	if (nspace.size() == 0)
		nspace = "default";

	if (IsAColumn(column))
	{
		string sql;
		sql = "select " + select_columns + string(" from tracks where ");
		sql += column + " like \"" + pattern + "\" and namespace like \"" + nspace + "\"";
		sql += " order by ";
		sql += ((orderby.size() == 0) ? column : orderby);
		sql += ";";
		InnerGetTracks(results, sql);
	}

}

bool DB::IsAColumn(std::string c)
{
	bool rv = false;
	if (c == "id") {
		rv = true;
	}
	else {
		for (size_t i = 0; i < sizeof(track_column_names) / sizeof(string); i++) {
			if (c == track_column_names[i]) {
				rv = true;
				break;
			}
		}
	}

	return rv;
}

/*	NOTE: nspace defaults to "default"
*/
int DB::GetArtistCount(string nspace)
{
	string sql("select count(*) from (select distinct artist from tracks where namespace like \'" + nspace + "\') as foo;");
	return IntegerQuery(sql);
}

bool DB::Initialized()
{
	return connection != nullptr;
}

/*	NOTE: nspace defaults to "default"
*/
string DB::PathFromID(unsigned int id, string * title, string * artist, string nspace)
{
	string rv;
	assert(Initialized());
	string sql("select parent,title,artist,fname from tracks where id = " + to_string(id) + " and namespace = \'" + nspace + "\';");
	sql::Statement * stmt = connection->createStatement();
	sql::ResultSet *res = nullptr;
	int up;

	try
	{
		if (stmt == nullptr)
			throw LOG2(_log_, "createStatement() failed", FATAL);

		res = stmt->executeQuery(sql.c_str());

		if (res->next())
		{
			up = res->getInt("parent");
			if (title != nullptr)
				*title = res->getString("title");
			if (artist != nullptr)
				*artist = res->getString("artist");
			rv = res->getString("fname");
		}

		// Rebuild the path on up. The idea is simple and yes, I read that it could be done
		// in one SQL statement. I saw several versions of it and didn't understand any of
		// them. So...
		//
		// Keep climbing up the file system path, prepending the current level's name at
		// each step. The file name itself is the initial value of rv. It is captured when
		// title and artist are captured, above.
		//
		// All the output of this function will be assembled into a PlayStruct.
		while (up >= 0)
		{
			sql = "select * from paths where me = " + to_string(up) + " and namespace = \'" + nspace + "\';";
			res = stmt->executeQuery(sql.c_str());
			if (res->next())
			{
				up = res->getInt("up");
				rv = res->getString("name") + "/" + rv;
			}
		}

	}
	catch (LoggedException s)
	{
		if (s.Level() == FATAL)
			throw s;
	}
	catch (sql::SQLException &e)
	{
		// These are always fatal.
		if (res != nullptr)
			delete res;
		ReformatSQLException(e);
	}

	// NOTE:
	// NOTE: THERE ARE SOME MEMORY LEAKS HERE - THESE WILL NOT BE FREED IF THERE ARE FATAL EXCEPTIONS.
	if (stmt != nullptr)
		delete stmt;
	if (res != nullptr)
		delete res;

	return rv;
}

void DB::FindIDs(std::string column, std::string pattern, std::vector<std::string> & results)
{
}

