//
//  parser.hpp
//  ParseOsmChangesetFile
//
//  Created by Bryce Cogswell on 1/2/23.
//  Copyright © 2023 Bryce Cogswell. All rights reserved.
//

#ifndef parser_hpp
#define parser_hpp

#include <stdio.h>
#include <vector>

class Changeset {
public:
	std::string date, user, application, comment;
	long ident;
	int uid, editCount;
	double min_lat, max_lat, min_lon, max_lon;
};

class ChangesetReader {
public:
	void virtual initialize() = 0;
	void virtual handleChangeset(const Changeset &) = 0;
	void virtual finalizeChangesets() = 0;
};

class ChangesetParser {
	std::vector<ChangesetReader *> readers;
public:
	void addReader(ChangesetReader * reader);
	bool parseXml( const char * s, const char * startDate );
};

#endif /* parser_hpp */
