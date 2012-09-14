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
 *  CapsuleDrawObject.h
 *  General CapsuleDrawObject to inherit from.
 *
 *  Created by Roemmermann on 21.10.09.
 */

#ifndef MARS_GRAPHICS_CAPSULE_DRAW_OBJECT_H
#define MARS_GRAPHICS_CAPSULE_DRAW_OBJECT_H

#include "DrawObject.h"

#include <interfaces/MARSDefs.h>
#include <utils/Vector.h>

#include <list>

namespace mars {
  namespace graphics {

    class CapsuleDrawObject : public DrawObject {

    public:
      CapsuleDrawObject(mars::interfaces::sReal radius, mars::interfaces::sReal height);
      virtual void setScaledSize(const mars::utils::Vector &scaledSize);

    protected:
      mars::interfaces::sReal radius_;
      mars::interfaces::sReal height_;
      osg::Geometry *geom;

      virtual std::list< osg::ref_ptr< osg::Geode > > createGeometry();
    }; // end of class CapsuleDrawObject

  } // end of namespace graphics
} // end of namespace mars

#endif /* MARS_GRAPHICS_CAPSULE_DRAW_OBJECT_H */
