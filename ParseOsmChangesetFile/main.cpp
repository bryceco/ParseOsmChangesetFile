//
//  main.cpp
//  ParseOsmChangesetFile
//
//  Created by Bryce on 7/4/14.
//  Copyright (c) 2014 Bryce Cogswell. All rights reserved.
//

#include <stdio.h>
#include <string>
#include <time.h>
#include <sys/time.h>

#include "ChangesetParser.hpp"
#include "Readers.hpp"


double timestamp()
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return time.tv_sec + time.tv_usec * 1e-6;
}

bool parseFile( const char * path, const char * startDate )
{
	printf("Start date = %s\n",startDate);
	printf("\n");

	ChangesetParser * parser = new ChangesetParser();
	auto readers = getReaders();
	for ( auto &reader: readers ) {
		parser->addReader(reader);
	}
	return parser->parseXmlFile( path, startDate );
}

int main(int argc, const char * argv[])
{
	const char * path;
	if ( argc == 2 ) {
		path = argv[1];
	} else {
		path = "/tmp/cs.osm";
	}
	const char * startDate = "2021-01-01";
	double time = timestamp();
	parseFile( path, startDate );
	time = timestamp() - time;
	printf( "total time = %f\n", time);
	return 0;
}
