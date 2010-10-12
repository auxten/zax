#ifndef VERSION_H
#define VERSION_H


	//Date Version Types
	static const char DATE[] = "21";
	static const char MONTH[] = "09";
	static const char YEAR[] = "2009";
	static const double UBUNTU_VERSION_STYLE = 9.09;
	
	//Software Status
	static const char STATUS[] = "Alpha";
	static const char STATUS_SHORT[] = "a";
	
	//Standard Version Type
	static const long MAJOR = 1;
	static const long MINOR = 0;
	static const long BUILD = 3;
	static const long REVISION = 15;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT = 112;
	#define RC_FILEVERSION 1,0,3,15
	#define RC_FILEVERSION_STRING "1, 0, 3, 15\0"
	static const char FULLVERSION_STRING[] = "1.0.3.15";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY = 3;
	

#endif //VERSION_h
