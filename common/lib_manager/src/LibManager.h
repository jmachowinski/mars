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

/**
 * \file LibManager.h
 * \author Malte Roemmermann
 * \brief "LibManager" is the class which holds the pointers to the LibInterface
 *        of loaded classes.
 *
 */

#ifndef LIB_MANAGER_H
#define LIB_MANAGER_H

#ifdef _PRINT_HEADER_
  #warning "LibManager.h"
#endif

#include "LibInterface.h"

#include <map>
#include <string>
#include <list>


namespace mars {
  namespace lib_manager {

    struct libStruct {
      LibInterface *libInterface;
      destroyLib *destroy;
      int useCount;
    };

    class LibManager {

      public:
        enum ErrorNumber {
          LIBMGR_NO_ERROR             = 0,
          LIBMGR_ERR_NO_LIBRARY       = 1,
          LIBMGR_ERR_LIBNAME_EXISTS   = 2,
          LIBMGR_ERR_NOT_ABLE_TO_LOAD = 3,
          LIBMGR_ERR_LIB_IN_USE       = 4
        };

        std::string errMessage[5];

        LibManager();
        ~LibManager();

        void addLibrary(LibInterface *_lib);
        ErrorNumber loadLibrary(const std::string &libPath, void *config = NULL);
        LibInterface* getLibrary(const std::string &libName);
        template <typename T> T* getLibraryAs(const std::string &libName);
        ErrorNumber unloadLibrary(const std::string &libPath);
        void loadConfigFile(const std::string &config_file);
        void getAllLibraries(std::list<LibInterface*> *libList);

      private:
        std::map<std::string, libStruct> libMap;

    }; // class LibManager

    // template implementations
    template <typename T>
      T* LibManager::getLibraryAs(const std::string &libName) {
      T *lib = NULL;
      LibInterface *libInterface = getLibrary(libName);
      if(libInterface){
        lib = dynamic_cast<T*>(libInterface);
      }
      return lib;
    }

  } // namespace lib_manager
} // namespace mars

#endif /* LIB_MANAGER_H */