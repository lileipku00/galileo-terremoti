//
// Created by ebassetti on 21/08/15.
//

#ifndef GALILEO_TERREMOTI_ACCELEROMETER_H
#define GALILEO_TERREMOTI_ACCELEROMETER_H

#include <string>

class Accelerometer {
public:
	virtual long getXAccel() = 0;
	virtual long getYAccel() = 0;
	virtual long getZAccel() = 0;
	virtual std::string getAccelerometerName() = 0;
};

#endif //GALILEO_TERREMOTI_ACCELEROMETER_H
