/*
 *  Copyright 2011, 2012, DFKI GmbH Robotics Innovation Center
 *
 *  This file is part of the MARS simulation framework.
 *
 *  MARS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3
 *  of the License, or (at your option) any later version.
 *
 *  MARS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with MARS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 *  NodeContactContactSensor.h
 *
 *  Created by Malte Roemmermann
 *
 */

#ifndef NODECONTACTFORCESENSOR_H
#define NODECONTACTFORCESENSOR_H

#ifdef _PRINT_HEADER_
#warning "NodeContactForceSensor.h"
#endif

#include "NodeArraySensor.h"

namespace mars {
  namespace sim {

    class NodeContactForceSensor : public NodeArraySensor {

    public:
      NodeContactForceSensor(interfaces::ControlCenter *control, IDListConfig config);
      ~NodeContactForceSensor(void) {}

      virtual int getAsciiData(char* data) const;
      virtual int getSensorData(interfaces::sReal** data) const;
      virtual void receiveData(const data_broker::DataInfo &info,
                               const data_broker::DataPackage &package,
                               int callbackParam);

      static interfaces::BaseSensor* instanciate(interfaces::ControlCenter *control,
                                           interfaces::BaseConfig *config);

    private:
      long contactForceIndex;
    };

  } // end of namespace sim
} // end of namespace mars

#endif