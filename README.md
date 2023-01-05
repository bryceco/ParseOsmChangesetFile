# ParseOsmChangesetFile
A fast parser for analyzing OpenStreetMap changeset history files.

Written for macOS, but should be easily ported to any *nix. 

OSM changeset history files are large XML files that require a long time to process using typical XML parsing libraries. As of early 2023 
the changeset file (https://planet.openstreetmap.org/planet/changesets-latest.osm.bz2) is over 62 GB when uncompressed. 

This project implements an app for gathering changeset statistics that are interesting to me, but includes a framework for
parsing changeset history files with minimal overhead, processing a 62 GB history file in 2-3 minutes 
on a MacBook Pro with M1 Pro processor and 32 GB memory. Fast processing times makes iterative refinement of analysis functions faster and easier.

Performance is improved using a combination of techniques:
* Written in C++.
* A custom miminmal XML parser designed solely for parsing changeset files (ChangesetParser.cpp).
* The history file is memory mapped, and there are no memory allocations for strings during processing, except for the specific 
values that are needed by the analysis functions.
* When the analysis only applies to changesets after a particular date (e.g. the last year) the raw XML file is binary searched for the
changeset at the cut-off date, avoiding the need to parse any XML before that date.

The parser is designed to be minimal but extensible. Rather than providing every piece of data that any analysis might need, you can add 
additional fields as needed by your analysis functions.

Here's an analysis function that counts and prints the total edits for every user in the history:
~~~
#include <iostream>
#include <map>
#include <string>
#include "ChangesetParser.hpp"
class UserEditCount: public ChangesetReader {
	std::map<std::string,long> userCounts;
	void initialize() {}
	void process(const Changeset & changeset)
	{
		auto it = userCounts.find(changeset.user);
		if ( it != userCounts.end() ) {
			it = userCounts.insert(std::pair<std::string,long>(changeset.user, 0)).first;
		}
		it->second += changeset.editCount;
	}
	void finalize()
	{
		for ( const auto &user: userCounts ) {
			std::cout << user.first << " = " << user.second << "\n";
		}
	}
};
~~~
and here's the main function that processes and analyzes everything after Jan 1, 2021:
~~~
int main(int argc, const char * argv[])
{
	const char * startDate = "2021-01-01";
	const char * path = NULL;
	if ( argc == 2 ) {
		path = argv[1];
	} else {
		return 1;
	}
	ChangesetParser * parser = new ChangesetParser();
	parser->addReader(new UserEditCount());
	if ( parser->parseXmlFile( path, startDate ) ) {
		return 0;
	} else {
		return 1;
	}
}
~~~
