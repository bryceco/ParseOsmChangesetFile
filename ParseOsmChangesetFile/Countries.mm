//
//  Countries.m
//  ParseOsmChangesetFile
//
//  Created by Bryce on 4/27/18.
//  Copyright © 2018 Bryce Cogswell. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#import "Countries.h"
#import "CountryJson.hpp"


@interface Countries : NSObject
@end


@implementation Countries
{
	id jsonDict;
}

-(instancetype)init
{
	self = [super init];
	if ( self ) {

		NSString * dataUtf8 = [NSString stringWithUTF8String:countryJson];
		NSData * data = [dataUtf8 dataUsingEncoding:NSUTF8StringEncoding];

		jsonDict = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];

		// NSArray * names = [self allNames];
		// NSLog(@"%@\n",names);
	}
	return self;
}

-(NSArray *)allNames
{
	NSMutableArray * a = [NSMutableArray new];
	NSArray * features = jsonDict[ @"features" ];
	for ( NSDictionary * feature in features ) {
		NSDictionary * properties = feature[ @"properties" ];
		NSString * name = properties[ @"NAME" ];
		[a addObject:name];
	}
	[a sortUsingComparator:^NSComparisonResult(NSString * s1, NSString * s2) {
		return [s1 caseInsensitiveCompare:s2];
	}];
	return a;
}

-(NSBezierPath *)pathForCountry:(NSString *)country
{
	NSArray * features = jsonDict[ @"features" ];
	for ( NSDictionary * feature in features ) {
		NSDictionary * properties = feature[ @"properties" ];
		NSString * name = properties[ @"NAME" ];
		if ( [name isEqualToString:country] ) {
			NSBezierPath * path = [NSBezierPath bezierPath];
			NSDictionary * geometry = feature[ @"geometry" ];
			NSArray * coordinates = geometry[ @"coordinates" ];
			for ( NSArray * shape in coordinates ) {
				for ( NSArray * polygon in shape ) {
					BOOL first = YES;
					for ( NSArray * pointPair in polygon ) {
						double x = [pointPair[0] doubleValue];
						double y = [pointPair[1] doubleValue];
						NSPoint point = NSMakePoint(x,y);
						if ( first ) {
							[path moveToPoint:point];
							first = NO;
						} else {
							[path lineToPoint:point];
						}
					}
				}
			}
			return path;
		}
	}
	return nil;
}
@end

#include <string>

bool CountryContainsPoint( const char * countryName, double lon, double lat )
{
	static std::string currentCountry;
	static NSBezierPath * currentPath = nil;

	if ( currentCountry != countryName ) {
		NSString * country = [NSString stringWithUTF8String:countryName];
		Countries * countries = [Countries new];
		currentCountry = countryName;
		currentPath = [countries pathForCountry:country];
	}
	return [currentPath containsPoint:NSMakePoint(lon,lat)];
}
