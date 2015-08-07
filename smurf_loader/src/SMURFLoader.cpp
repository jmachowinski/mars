/*
 *  Copyright 2012, 2014, DFKI GmbH Robotics Innovation Center
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
 *
 * \file SMURFLoader.cpp
 * \author Malte Langosz, Kai von Szadkowski
 */

#include "SMURFLoader.h"
#include "zipit.h"

#include <lib_manager/LibManager.hpp>
#include <lib_manager/LibInterface.hpp>
#include <mars/interfaces/sim/SimulatorInterface.h>
#include <mars/interfaces/sim/EntityManagerInterface.h>
#include <mars/sim/SimEntity.h>
#include <mars/utils/misc.h>
#include <mars/utils/mathUtils.h>


namespace mars {
  namespace smurf {

    using namespace mars::utils;
    using namespace mars::interfaces;


    SMURFLoader::SMURFLoader(lib_manager::LibManager *theManager) :
      interfaces::LoadSceneInterface(theManager), control(NULL){

      mars::interfaces::SimulatorInterface *marsSim;
      marsSim = libManager->getLibraryAs<mars::interfaces::SimulatorInterface>("mars_sim");
      if(marsSim) {
        control = marsSim->getControlCenter();
        control->loadCenter->loadScene[".zsmurf"] = this; // zipped smurf model
        control->loadCenter->loadScene[".zsmurfs"] = this; // zipped smurf scene
        control->loadCenter->loadScene[".smurf"] = this; // smurf model
        control->loadCenter->loadScene[".smurfs"] = this; // smurf scene
        control->loadCenter->loadScene[".svg"] = this; // smurfed vector graphic
        control->loadCenter->loadScene[".urdf"] = this; // urdf model
        LOG_INFO("smurf_loader: added SMURF loader to loadCenter");
      }

      factoryManager =
            theManager->acquireLibraryAs<mars::entity_generation::EntityFactoryManager>(
                "mars_entity_factory");
    }

    SMURFLoader::~SMURFLoader() {
      if(control) {
        control->loadCenter->loadScene.erase(".zsmurf");
        control->loadCenter->loadScene.erase(".zsmurfs");
        control->loadCenter->loadScene.erase(".smurf");
        control->loadCenter->loadScene.erase(".smurfs");
        control->loadCenter->loadScene.erase(".svg");
        control->loadCenter->loadScene.erase(".urdf");
        libManager->releaseLibrary("mars_sim");
      }
      libManager->releaseLibrary("mars_entity_factory");
    }

    void SMURFLoader::checkEncodings() {
            bool existing = true;
            QString str("<xml><easter_egg>3.1418</easter_egg></xml>");
            QString tmpFilename = QString(tmpPath.c_str()) + QString("mars-encoding-check");
            QFile tmpFile(tmpFilename);
            if (!QFile::exists(tmpFilename)) {
              tmpFile.open(QIODevice::WriteOnly);
              QTextStream out(&tmpFile);
              out << str;
              tmpFile.close();
              existing = false;
            }
            if (!tmpFile.open(QIODevice::ReadOnly)) {
              LOG_FATAL("Cannot open language checking file\n");
              exit(-1);
            }
            QDomDocument doc;
            if (!doc.setContent(&tmpFile, false)) {
              LOG_FATAL("Cannot parse language checking file\n");
              exit(-2);
            }
            QDomElement root = doc.documentElement();
            QDomElement tmpElement;
            if (root.elementsByTagName(QString("easter_egg")).at(0).toElement().text().toDouble()
                != 3.1418) {
              LOG_ERROR(
                  "Encoding of the system is invalid, therefore Scene loading will fail quitting here to prevent errors later");
              exit(-3);
            }
            tmpFile.close();
            if (!existing) {
              tmpFile.remove();
            }
          }

          void SMURFLoader::getGenericConfig(configmaps::ConfigMap *config,
              const QDomElement &elementNode) {

            QDomNodeList xmlnodepartlist = elementNode.childNodes();
            QDomNamedNodeMap attributes = elementNode.attributes();

            std::string tagName, value;
            QDomElement child;
            QDomNode child2;

            for (int i = 0; i < attributes.size(); i++) {
              child2 = attributes.item(i);
              tagName = child2.nodeName().toStdString();
              value = child2.nodeValue().toStdString();
              if (!tagName.empty()) {
                (*config)[tagName].push_back(configmaps::ConfigItem(value));
    #ifdef DEBUG_PARSE_SENSOR
                LOG_DEBUG("attrib [%s : %s]", tagName.c_str(), value.c_str());
    #endif
              }
            }

            for (int i = 0; i < xmlnodepartlist.size(); i++) {
              child = xmlnodepartlist.at(i).toElement();

              tagName = child.tagName().toStdString();
              value = child.text().toStdString();
              if (!tagName.empty()) {
                (*config)[tagName].push_back(configmaps::ConfigItem(value));
    #ifdef DEBUG_PARSE_SENSOR
                LOG_DEBUG("element [%s : %s]", tagName.c_str(), value.c_str());
    #endif
                getGenericConfig(&((*config)[tagName].back().children), child);
              }
            }

            // we can or should also iterate over the attributes
          }

          void SMURFLoader::getGenericConfig(std::vector<configmaps::ConfigMap> *configList,
              const QDomElement &elementNode) {
            configmaps::ConfigMap config;
            getGenericConfig(&config, elementNode);
            configList->push_back(config);
          }

          unsigned int SMURFLoader::parseSVG(std::vector<configmaps::ConfigMap> *configList,
              std::string sceneFilename) {
            checkEncodings();
            //  HandleFileNames h_filenames;
            std::vector<std::string> v_filesToLoad;
            QString xmlErrorMsg = "";
            int xmlErrorLine, xmlErrorCol = 0;

            //creating a handle for the xmlfile
            QFile file(sceneFilename.c_str());
            std::string path = getPathOfFile(sceneFilename);

            QLocale::setDefault(QLocale::C);

            LOG_INFO("Load: loading scene: %s", sceneFilename.c_str());

            //test to open the xmlfile
            if (!file.open(QIODevice::ReadOnly)) {
              std::cout << "Error while opening scene file content " << sceneFilename
                  << " in Load.cpp->parseScene" << std::endl;
              std::cout << "Make sure your scenefile name corresponds to"
                  << " the name given to the enclosed .scene file" << std::endl;
              return 0;
            }

            //test to pass the content from the xmlfile to the DOM-Object
            QDomDocument doc;
            if (!doc.setContent(&file, false, &xmlErrorMsg, &xmlErrorLine, &xmlErrorCol)) {
              file.close();
              std::cout << "error passing the file content in->Load.cpp->parseScene" << std::endl;
              std::cout << "Message: " << xmlErrorMsg.toStdString() << "\n" << "Line: " << xmlErrorLine
                  << "\n" << "Collumn: " << xmlErrorCol << "\n" << std::endl;
              return 0;
            }

            //the XML-file has been opend and is ready for parsing it's content
            //the first element is the "root" element
            QDomElement root = doc.documentElement();
            QDomElement tmpElement;
            double version = 0.0;

            QDomNodeList xmlnodelist = root.elementsByTagName(QString("version"));
            if (!xmlnodelist.isEmpty()) {
              tmpElement = xmlnodelist.at(0).toElement();
              version = tmpElement.text().toDouble();
            }
            if (version < 0.199999) {
              LOG_WARN("Load: old SceneFile version. Take care of the following changes:");
              LOG_WARN("\t- the sphere object get radius as size instead of diameter");
              LOG_WARN(
                  "\t- the origin numbers for primitives are replaced by strings (box, shpere, etc.)");
              LOG_WARN(
                  "\t- the sensor type is a string instead of a number, like NodePosition, JointPosition, or NodeCOM");
              LOG_WARN("\t- the JOint6DOF sensor gets nodeID and jointID instead of id list");
              LOG_WARN("\t- the actual_pos and acutal_rot is removed from nodes");
            }

            global_width = root.attribute("width").toDouble();
            global_length = root.attribute("height").toDouble();

    //  std::vector<configmaps::ConfigMap> poiList;
            //first checking wether there is a node with name "node"
            //by passing it in a xmlnodelist and checking if it's not empty.
            //if so, there is at least an element with name "node"

            int start = 0;
            int end = 0;
            double angle = 0.0;
            std::string transform = "";
            std::string originstring;
            std::vector<std::string> transform_elements;
            std::vector<std::string> originelements;
            std::vector<std::string> descelements;
            std::string description;
            double origin_z = 0;
            QDomElement rect;
            QDomElement originpath;
            Vector origvec;
            xmlnodelist = root.elementsByTagName(QString("g"));
            Quaternion orientation;
            Vector translation;
            for (int i = 0; i < xmlnodelist.size(); i++) {
              //getGenericConfig(&map, xmlnodelist.at(i).toElement());
              ConfigMap map;
              //// TODO: do this if on Windows
              //std::string current_locale_text = qs.toLocal8Bit().constData();
              map["file"] = path + xmlnodelist.at(i).toElement().elementsByTagName("title").at(0).toElement().text().toUtf8().constData();
              description = xmlnodelist.at(i).toElement().elementsByTagName("desc").at(0).toElement().text().toUtf8().constData();
              descelements = explodeString(' ', description);
              if (descelements.at(0) == "origin_z:") {
                origin_z = ::atof(descelements.at(1).c_str());
              }
              //TODO make this generic
              transform = xmlnodelist.at(i).toElement().attribute("transform", "").toUtf8().constData();
              if (transform == "") {
                translation = Vector(0, 0, 0);
                angle = 0;
                fprintf(stderr, "No transform for entity found.");
              } else {
                start = transform.find("(");
                end = transform.find(")");
                transform_elements = explodeString(',', transform.substr(start + 1, end - start - 1));
                fprintf(stderr, "Found transform with %i elements.", (int)transform_elements.size());
                if (transform_elements.size() > 4) {
                  translation = Vector(::atof(transform_elements.at(4).c_str()),
                       ::atof(transform_elements.at(5).c_str()), 0.0);
                  double sinus = ::atof(transform_elements.at(1).c_str());
                  double cosinus = ::atof(transform_elements.at(0).c_str());
                  angle = atan2(sinus, cosinus);
                  fprintf(stderr, "angle: %g", angle);
                } else {
                  if (transform_elements.size() == 2) {

                    translation = Vector(::atof(transform_elements.at(0).c_str()),
                        ::atof(transform_elements.at(1).c_str()), 0.0);

                  } else {
                    translation = Vector(0, 0, 0);
                  }
                  angle = 0.0;
                }
              }
              map["orientation_z"] = radToDeg(-angle);
              orientation = utils::eulerToQuaternion(Vector(0, 0, radToDeg(angle)));
              //orientation = Quaternion(orientation.w(), orientation.x(), -orientation.y(), orientation.z());

              rect = xmlnodelist.at(i).toElement().elementsByTagName("rect").at(0).toElement();
              originpath = xmlnodelist.at(i).toElement().elementsByTagName("path").at(0).toElement();
              originelements = explodeString(',',
                  explodeString(' ', originpath.attribute("d").toUtf8().constData()).at(1));
              fprintf(stderr, "Originvector x: %s, y: %s, angle: %g\n", ((std::string) originelements.at(0)).c_str(),
                  ((std::string) originelements.at(1)).c_str(), radToDeg(angle));
              origvec = Vector(::atof(originelements.at(0).c_str()),
                  ::atof(originelements.at(1).c_str()), 0.0);
              origvec = translation + (orientation * origvec);
              //origvec = orientation * (translation +  origvec);
              map["origin_x"] = origvec(0);
    //          if (angle > 0.001)
    //            origvec(1) = global_width - origvec(1);
    //          if (angle < -0.001)
    //            map["origin_y"] = origvec(1) - global_width;
    //          else
              map["origin_y"] = global_width - origvec(1);
              //map["origin_y"] = origvec(1);
              map["origin_z"] = origin_z;
              fprintf(stderr, "%g, %g\n", origvec(0), origvec(1));
              //map["field_width"] = rect.attribute("height").toDouble();
              //map["field_length"] = rect.attribute("width").toDouble();
              configList->push_back(map);
            }
    //
    //        for (std::vector<configmaps::ConfigMap>::iterator it = entityList.begin();
    //            it != entityList.end(); ++it) {
    //          map = ConfigMap::fromYamlFile((std::string) (*it)["id"] + ".yml", true);
    //          if (it->find("transform") != it->end()) {
    //            //fprintf(stderr, ((std::string) (*it)["id"] + ".yml").c_str());
    //
    //            transform = (std::string) (*it)["transform"];
    //            start = transform.find("(");
    //            end = angle.find(")");
    //
    //            fprintf(stderr, "\n transform: %g\n", (double) map["orientation_z"]);
    //          }
    //
    //          //map["orientation_z"] = ::atof(angle.c_str());
    //          //fprintf(stderr, ((std::string)(*it)["width"]+"\n").c_str());//, ((std::string)(*it)["width"]).c_str(), "\n");
    //        }

    //  xmlnodelist = root.elementsByTagName(QString("square"));
    //  for (int i = 0; i < xmlnodelist.size(); i++) {
    //    getGenericConfig(&poiList, xmlnodelist.at(i).toElement());
    //  }

            file.close();
            return 1;
        }

    /**
       * Loads a URDF, SMURF or SMURFS (SMURF scene) file.
       * @param filename The name of the file to be loaded as a full path.
       * @param tmpPath The path to the temporary directory to which files should be unzipped
       *        if the file format requires it.
       * @param robotname This parameter is obsolete with URDF and SMURF and was only
       *        needed for MARS scenes. It may be removed in the future.
       * @return 1 if the file was successfully loaded, 0 otherwise
       */
    //TODO: remove parameter "robotname"
    bool SMURFLoader::loadFile(std::string filename, std::string tmpPath,
                              std::string robotname) {
      LOG_INFO("urdf_loader: prepare loading");
      entitylist.clear();

      // split up filename in path + _filename and retrieve file extension
      std::string file_extension = utils::getFilenameSuffix(filename);
      fprintf(stderr, "SMURFLoader::loadFile: file_extension=%s\n", file_extension.c_str());
      std::string path = utils::getPathOfFile(filename);
      std::string _filename = filename;
      utils::removeFilenamePrefix(&_filename);

      // need to unzip into a temporary directory
      if (file_extension == ".zsmurf" or file_extension == ".zsmurfs") {
        if (unzip(tmpPath, filename) == 0) {
          path = tmpPath;
          return 0;
        }
      }

      // read in the provided file - .smurfs / .smurf / .urdf
      configmaps::ConfigMap map;
      std::string uri;
      std::string uri_extension;
      fprintf(stderr, "Reading in %s...\n", (path+_filename).c_str());
      if(file_extension == ".smurfs") {
        configmaps::ConfigVector::iterator it;
        map = configmaps::ConfigMap::fromYamlFile(path+_filename, true);
        map.toYamlFile("smurfs_debugmap.yml");
        for (it = map["smurfs"].begin(); it != map["smurfs"].end(); ++it) { // backwards compatibility
          uri = (std::string)(*it)["file"];
          if (uri == "") {
            uri = (std::string)(*it)["URI"]; // backwards compatibility
          }
          uri_extension = utils::getFilenameSuffix(uri);
          // get type from file extension if there is no type
          if ((std::string)(*it)["type"] == "")
            (*it)["type"] = uri_extension.substr(1);
          // handle absolute paths
          if (uri.substr(0,1) == "/") {
            (*it)["path"] = utils::getPathOfFile(uri);
          } else {
            (*it)["path"] = path+utils::getPathOfFile(uri);
          }
          std::string fulluri = uri;
          utils::removeFilenamePrefix(&uri);
          (*it)["file"] = uri;
          // the following allows adding an old MARS scene file in a smurf scene
          if (((std::string)(*it)["type"] == "scn") || ((std::string)(*it)["type"] == "scene") || ((std::string)(*it)["type"] == "yml")) {
            control->loadCenter->loadScene[uri_extension]->loadFile(fulluri,
                path, (std::string)(*it)["name"]);
          }
          else {
            entitylist.push_back((*it).children);
          }
        }
        for (it = map["entities"].begin(); it != map["entities"].end(); ++it) { // new tag
          uri = (std::string)(*it)["file"];
          if (uri == "") {
            uri = (std::string)(*it)["URI"]; // backwards compatibility
          }
          uri_extension = utils::getFilenameSuffix(uri);
          // get type from file extension if there is no type
          if ((std::string)(*it)["type"] == "")
            (*it)["type"] = uri_extension.substr(1);
          // handle absolute paths
          if (uri.substr(0,1) == "/") {
            (*it)["path"] = utils::getPathOfFile(uri);
          } else {
            (*it)["path"] = path+utils::getPathOfFile(uri);
          }
          std::string fulluri = uri;
          utils::removeFilenamePrefix(&uri);
          (*it)["file"] = uri;
          // the following allows adding an old MARS scene file in a smurf scene
          if (((std::string)(*it)["type"] == "scn") || ((std::string)(*it)["type"] == "scene")) {
            control->loadCenter->loadScene[uri_extension]->loadFile(fulluri,
                path, (std::string)(*it)["name"]);
          }
          else {
            entitylist.push_back((*it).children);
          }
        }
      } else if(file_extension == ".smurf") {
        // if we have only one smurf, only one with rudimentary data is added to the smurf list
          //map["URI"] = _filename;
          // map = configmaps::ConfigMap::fromYamlFile(path+_filename, true);
          map["path"] = path;
          map["file"] = _filename;
          map["type"] = "smurf";
          map["name"] = "";
          entitylist.push_back(map);
      } else if(file_extension == ".urdf") {
          map["file"] = _filename;
          map["path"] = path;
          map["name"] = "";
          map["type"] = "urdf";
          entitylist.push_back(map);
      } else if(file_extension == ".svg") {
        /* A smurf svg can contain multiple entities, thus we have to loop over them.
         * Each svg object must contain a "file" property, specifying the yaml file in
         * which the missing parameters are defined, so we have to append that.
         */
        std::vector<configmaps::ConfigMap> svg_entities;
        parseSVG(&svg_entities, path+_filename);
        for(std::vector<configmaps::ConfigMap>::iterator it = svg_entities.begin();
            it!=svg_entities.end(); ++it) {
          map = *it;
          map.append(ConfigMap::fromYamlFile((std::string)map["file"], true)); //FIXME: path?
          fprintf(stderr, "Loading config for svg entity: %s", ((std::string)map["file"]).c_str());
          entitylist.push_back(map);
        }
      }

      //load assembled smurfs
      fprintf(stderr, "Creating simulation entities...\n");
      for (std::vector<configmaps::ConfigMap>::iterator sit = entitylist.begin();
          sit != entitylist.end(); ++sit) {
        configmaps::ConfigMap tmpmap;
        tmpmap = *sit;
        factoryManager->createEntity(tmpmap);
      }

      return 1; //TODO: check number of successfully loaded entities before returning 1
    }

    int SMURFLoader::saveFile(std::string filename, std::string tmpPath) {
      return 0;
    }


    unsigned int SMURFLoader::unzip(const std::string& destinationDir,
                                     const std::string& zipFilename) {
      if (!utils::createDirectory(destinationDir))
        return 0;

      Zipit myZipFile(zipFilename);
      LOG_INFO("Load: unsmurfing zipped SMURF: %s", zipFilename.c_str());

      if (!myZipFile.unpackWholeZipTo(destinationDir))
        return 0;

      return 1;
    }

  } // end of namespace smurf
} // end of namespace mars

DESTROY_LIB(mars::smurf::SMURFLoader);
CREATE_LIB(mars::smurf::SMURFLoader);
