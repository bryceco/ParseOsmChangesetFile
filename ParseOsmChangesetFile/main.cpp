//
//  main.cpp
//  ParseOsmChangesetFile
//
//  Created by Bryce on 7/4/14.
//  Copyright (c) 2014 Bryce Cogswell. All rights reserved.
//


#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <algorithm>

#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "Countries.h"
#include "ChangesetParser.hpp"

double timestamp()
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return time.tv_sec + time.tv_usec * 1e-6;
}

double GreatCircleDistance(double lon1, double lat1, double lon2, double lat2)
{
	double EarthRadius = 6378137.0;

	// haversine formula
	double dlon = (lon2 - lon1) * M_PI / 180;
	double dlat = (lat2 - lat1) * M_PI / 180;
	double a = pow(sin(dlat / 2), 2) + cos(lat1 * M_PI / 180) * cos(lat2 * M_PI / 180) * pow(sin(dlon / 2), 2);
	double c = 2 * atan2(sqrt(a), sqrt(1 - a));
	double meters = EarthRadius * c;
	return meters;
}

class EditorInfoReader: public ChangesetReader {
	struct EditorInfo {
		long					changesets;
		long					edits;
		std::set<std::string>	usersPerDay;
		long					uniqueUsersPerDaySum;
	};
	typedef std::map<std::string,EditorInfo> EditorMap;	// map editor name to info
	EditorMap		editors;

	std::string		prevDate = "";
	long			dateCount = 0;

	void initialize() {
	}

	void handleChangeset(const Changeset & changeset)
	{
		if ( changeset.date != prevDate ) {
			prevDate = changeset.date;
			for ( auto &editor_pair : editors ) {
				auto &editor = editor_pair.second;
				editor.uniqueUsersPerDaySum += editor.usersPerDay.size();
				editor.usersPerDay.clear();
			}
			++dateCount;
		}

		auto it = editors.find(changeset.application);
		if ( it == editors.end() ) {
			it = editors.insert( std::pair<std::string,EditorInfo>(changeset.application, EditorInfo()) ).first;
		}
		EditorInfo & e = it->second;
		e.usersPerDay.insert( changeset.user );
		e.edits += changeset.editCount;
		e.changesets += 1;
	}

	void finalizeChangesets()
	{
		// print average number of daily users for each editor
		printf("\n");
		printf( "Average daily users for editors:\n");

		struct stats {
			double user_rate;
			double edit_rate;
			std::string	editor;
			bool operator<(const stats& a) const {
				return user_rate < a.user_rate;
			}
		};

		std::vector<stats>	list;
		for ( const auto &editor_pair: editors ) {
			const auto &editor = editor_pair.second;
			stats s = {
				(double)editor.uniqueUsersPerDaySum / dateCount,
				editor.edits / (double)editor.uniqueUsersPerDaySum / dateCount,
				editor_pair.first.c_str()
			};
			list.push_back(s);
		}
		std::sort( list.begin(), list.end() );
		std::reverse( list.begin(), list.end() );

		for ( const auto &item: list ) {
			if ( item.user_rate > 0.1 ) {
				printf( "%-30s %6.1f %12.1f\n",
					   item.editor.c_str(),
					   item.user_rate,
					   item.edit_rate );
			}
		}
	}
};


class LargeAreaReader: public ChangesetReader {
	typedef std::map<std::string,long>	LargeAreaMap;	// for each editor count the number of large changesets
	LargeAreaMap	largeAreaMap;

	void initialize() {}
	void handleChangeset(const Changeset & changeset)
	{
		if ( GreatCircleDistance(changeset.min_lon, changeset.min_lat, changeset.max_lon, changeset.max_lat) > 1000*1000.0 ) {
			std::pair<LargeAreaMap::iterator,bool> result = largeAreaMap.insert(std::pair<std::string,long>(changeset.application,1));
			if ( !result.second ) {
				result.first->second += 1;
			}
		}
	}

	void finalizeChangesets()
	{
		// print large edit area counts
		 printf("\n");
		 printf( "Number of large changeset areas:\n");
		 for ( LargeAreaMap::iterator editor = largeAreaMap.begin(); editor != largeAreaMap.end(); ++editor ) {
			 long rate = editor->second;
			 printf( "%-30s %6ld\n", editor->first.c_str(), rate );
		 }
	}
};


class EditsPerUserReader: public ChangesetReader {
	struct UserEditCount {
		long			changesetCount;
		long			editCount;
		long			changesetInChinaCount;
		long			editInChinaCount;
		std::string		lastDate;
		long			lastChangesetId;
		UserEditCount() : editCount(0), changesetCount(0), changesetInChinaCount(0), editInChinaCount(0) {}
	};
	typedef std::map<std::string,UserEditCount>	PerEditorUserMap;	// map user-name to edit stats

	typedef std::map<std::string,PerEditorUserMap> PerEditorMap; // map editor name to stats
	PerEditorMap perEditorMap;

	void initialize() {
		perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("Go Map!!",PerEditorUserMap()));
		perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("Vespucci",PerEditorUserMap()));
		perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("StreetComplete",PerEditorUserMap()));
		perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("MapComplete",PerEditorUserMap()));
	}

	void handleChangeset(const Changeset & changeset)
	{
		auto it = perEditorMap.find( changeset.application );
		if ( it != perEditorMap.end() ) {
			PerEditorUserMap & perEditor = it->second;


			auto it = perEditor.find(changeset.application);
			if ( it == perEditor.end() ) {
				it = perEditor.insert( std::pair<std::string,UserEditCount>(changeset.user,UserEditCount()) ).first;
			}
			UserEditCount & editsForUser = it->second;
			editsForUser.changesetCount	+= 1;
			editsForUser.editCount		+= changeset.editCount;
			editsForUser.lastDate		 = changeset.date;
			editsForUser.lastChangesetId = changeset.ident;

		#if 0
			if ( CountryContainsPoint( "China", min_lon, min_lat ) &&
				 CountryContainsPoint( "China", min_lon, max_lat ) &&
				 CountryContainsPoint( "China", max_lon, min_lat ) &&
				 CountryContainsPoint( "China", max_lon, max_lat ) )
			{
				printf( "China: %s %ld: %s -- %f, %f, %f, %f\n", changesetUser.c_str(), changesetId, changesetDate.c_str(), min_lat, max_lat, min_lon, max_lon );
				editsForUser.editInChinaCount += editCount;
				editsForUser.changesetInChinaCount += 1;
			}
		#endif
		}
	}

	void finalizeChangesets()
	{
		// print number of edits each user of Go Map made
		for ( const auto &it: perEditorMap ) {
			const char * editorName = it.first.c_str();
			const PerEditorUserMap & perEditor = it.second;
			printf( "\n");
			printf( "%s most prolific users:\n", editorName);
			struct PerEditorUser {
				const std::string		name;
				UserEditCount			count;
				PerEditorUser( const std::string & name, UserEditCount count ) : name(name), count(count) {}
				bool operator < (const PerEditorUser & other) const	{ return count.lastDate < other.count.lastDate;	}
			};
			std::list<PerEditorUser> perEditorUserVector;
			long totalEdits = 0;
			long totalChangesets = 0;
			for ( const auto &user: perEditor ) {
				perEditorUserVector.push_back(PerEditorUser(user.first,user.second));
				totalEdits += user.second.editCount;
				totalChangesets += user.second.changesetCount;
			}

			perEditorUserVector.sort( [](PerEditorUser const& a, PerEditorUser const& b) { return a.count.editCount > b.count.editCount; });
			while ( perEditorUserVector.size() > 100 ) {
				perEditorUserVector.pop_back();
			}
			// add totals
			perEditorUserVector.push_front(PerEditorUser("<Total>",UserEditCount()));
			perEditorUserVector.front().count.lastDate = "          ";

			printf( "   sets     edits  most recent     last set\n");
			for ( const auto &user: perEditorUserVector ) {
				if ( user.count.editCount > 0 ) {
					printf( "%7ld %9ld   %s  %11ld  %s\n",
						   user.count.changesetCount,
						   user.count.editCount,
						   user.count.lastDate.c_str(),
						   user.count.lastChangesetId,
						   user.name.c_str() );
				}
			}
		}
	}
};


class GoMapReader: public ChangesetReader {

	void initialize() {}
	void handleChangeset(const Changeset & changeset)
	{
	}

	void finalizeChangesets()
	{
	}
};



// Track the most common changeset comments
class ChangesetCommentReader: public ChangesetReader {
	typedef std::map<std::string,long> ChangesetCommentMap;
	ChangesetCommentMap comments;

	typedef std::pair<std::string,long> ChangesetCount;
	static bool compare(const ChangesetCount & a, const ChangesetCount & b)
	{
		return a.second > b.second;
	}

	void initialize() {}
	void handleChangeset(const Changeset & changeset)
	{
		auto it = comments.find(changeset.application);
		if ( it == comments.end() ) {
			it = comments.insert( std::pair<std::string,long>(changeset.comment, 0) ).first;
		}
		it->second++;
	}

	void finalizeChangesets()
	{
		// print changeset comments
		std::vector<ChangesetCount> changesetVector;
		changesetVector.reserve( comments.size());
		for ( const auto & c: comments ) {
			changesetVector.push_back(ChangesetCount(c.first,c.second));
		}
		std::sort(changesetVector.begin(),changesetVector.end());
		printf("\n");
		printf("Top 100 changeset comments:\n");
		for ( int i = 0; i < 100; ++i ) {
			const ChangesetCount & c = changesetVector[ i ];
			double percent = 100.0 * c.second / changesetVector.size();
			printf("%9ld (%.6f%%) %s\n", c.second, percent, c.first.c_str());
		}
	}
};

// Print the current date each time we reach a new year parsing the changeset file
class DatePrinterReader: public ChangesetReader {
	std::string prev = "";

	void initialize() {}
	void handleChangeset(const Changeset & changeset)
	{
		if ( prev.length() >= 4 && prev[3] != changeset.date[3] && changeset.date >= "2010" ) {
			printf("%s\n",changeset.date.c_str());
		}
		prev = changeset.date;
	}
	void finalizeChangesets()
	{
	}
};

// Track the number of times each comment is used by StreetComplete users
class StreetCompleteCommentReader: public ChangesetReader {
	typedef std::map<std::string,long> StreetCompleteCommentMap;
	StreetCompleteCommentMap comments;

	void initialize() {}
	void handleChangeset(const Changeset & changeset)
	{
		if ( changeset.application == "StreetComplete" ) {
			auto it = comments.find(changeset.application);
			if ( it == comments.end() ) {
				it = comments.insert( std::pair<std::string,long>(changeset.comment, 0) ).first;
			}
			it->second++;
		}
	}

	void finalizeChangesets()
	{
		long scTotal = 0;
		std::vector<std::pair<long,std::string>> scComments;
		for (const auto & c: comments ) {
			scComments.push_back(std::pair<long,std::string>(c.second,c.first));
			scTotal += c.second;
		}
		std::sort( scComments.begin(), scComments.end() );
		std::reverse( scComments.begin(), scComments.end() );
		printf("\n");
		printf("StreetComplete comments:\n");
		double acc = 0.0;
		for ( const auto & c: scComments ) {
			acc += c.first;
			printf("%-9ld %.2f%% (%.2f%%) %s\n",
				   c.first,
				   100.0*c.first/scTotal,
				   100.0*acc/scTotal,
				   c.second.c_str());
		}
	}
};

bool parseXml( const char * xmlString, const char * startDate )
{
	printf("Start date = %s\n",startDate);
	printf("\n");

	ChangesetParser * parser = new ChangesetParser();
	parser->addReader(new DatePrinterReader());
	parser->addReader(new EditorInfoReader());
	parser->addReader(new EditsPerUserReader());
	parser->addReader(new ChangesetCommentReader());
	parser->addReader(new StreetCompleteCommentReader());

	parser->parseXml( xmlString, startDate );

	return true;
}

bool parsePath( const std::string & path )
{
	if ( path.compare(path.length()-4, 4, ".bz2") == 0 ) {
		return false;
	}

	int fd = open( path.c_str(), O_RDONLY );
	if ( fd < 0 ) {
		perror("");
		return false;
	}
	struct stat statbuf = { 0 };
	fstat(fd,&statbuf);

	const void * mem = mmap(NULL, statbuf.st_size, PROT_READ, MAP_FILE|MAP_SHARED|MAP_NOCACHE, fd, 0);
	madvise( (void*)mem, statbuf.st_size, MADV_SEQUENTIAL );	// 222 without

	bool ok = parseXml( (const char *)mem, "2021-01-01" );

	munmap( (void *)mem, statbuf.st_size);
	return ok;
}


int main(int argc, const char * argv[])
{
	double time = timestamp();
	if ( argc == 2 ) {
		parsePath( argv[1] );
	} else {
		parsePath( "/tmp/cs.osm" );
	}
	time = timestamp() - time;
	printf( "total time = %f\n", time);
	return 0;
}
