/*
 *  Copyright 2011, 2012, 2014, DFKI GmbH Robotics Innovation Center
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

#include "CameraSensor.h"

#include <mars/data_broker/DataBrokerInterface.h>
#include <mars/utils/mathUtils.h>
#include <mars/interfaces/sim/LoadCenter.h>
#include <mars/interfaces/sim/NodeManagerInterface.h>
#include <mars/interfaces/sim/SimulatorInterface.h>
#include <mars/interfaces/sim/ControlCenter.h>
#include <mars/interfaces/graphics/GraphicsManagerInterface.h>
#include <mars/interfaces/Logging.hpp>

#include <stdint.h>
#include <cstring>
#include <cstdlib>


namespace mars {
  namespace sim {

    using namespace utils;
    using namespace interfaces;

    BaseSensor* CameraSensor::instanciate(ControlCenter *control, BaseConfig *config ){
      CameraConfigStruct *cfg = dynamic_cast<CameraConfigStruct*>(config);
      assert(cfg);
      return new CameraSensor(control,*cfg);
    }


    CameraSensor::CameraSensor(ControlCenter *control, const CameraConfigStruct config) :
      BaseNodeSensor(config.id,config.name),
      SensorInterface(control),
      config(config),
      depthCamera(id,name,config.width,config.height,1,true),
      imageCamera(id,name,config.width,config.height,4,false)
    {
      renderCam = 2;
      this->attached_node = config.attached_node;
      std::vector<unsigned long>::iterator iter;
      dbPosIndices[0] = -1;

      control->nodes->addNodeSensor(this);
      std::string groupName, dataName;
      this->config.ori_offset = this->config.ori_offset * eulerToQuaternion(Vector(90,0,-90)); //All elements should be X Forwart looging to meet rock-convention, so i add this offset for all setting

      bool erg = control->nodes->getDataBrokerNames(attached_node, &groupName, &dataName);
      assert(erg);
      if(control->dataBroker->registerTimedReceiver(this, groupName, dataName,"mars_sim/simTimer", config.updateRate)) {
      }
      

      cam_id=0;
      if(control->graphics) {

        //New
        interfaces::hudElementStruct hudCam;
        hudCam.type            = HUD_ELEMENT_TEXTURE;
        hudCam.width           = config.hud_width;
        hudCam.height          = config.hud_height;
        hudCam.texture_width   = hudCam.width;
        hudCam.texture_height  = hudCam.height;
        hudCam.posx            = 40 + (hudCam.width * config.hud_pos); // aligned in a row
        hudCam.posy            = 30;
        hudCam.border_color[0] = 0.0;
        hudCam.border_color[1] = 0.58824;
        hudCam.border_color[2] = 0.0;
        hudCam.border_color[3] = 1.0;
        hudCam.border_width    = 5.0;

        if(config.show_cam)
          cam_id = control->graphics->addHUDElement(&hudCam);

       
        cam_window_id = control->graphics->new3DWindow(0, true, config.width,
                                                       config.height, name);
        if(config.show_cam)
          control->graphics->setHUDElementTextureRTT(cam_id, cam_window_id,
                                                     false);

        gw = control->graphics->get3DWindow(cam_window_id);
        gw->setGrabFrames(false);
        if(gw) {
          gc = gw->getCameraInterface();
          control->graphics->addGraphicsUpdateInterface(this);
          gc->setFrustumFromRad(config.opening_width/180.0*M_PI, config.opening_height/180.0*M_PI, 0.5, 100);
        }
      }
      
      if(!this->config.enabled){
        control->graphics->deactivate3DWindow(cam_window_id);
      }
      
    }

    CameraSensor::~CameraSensor(void){
      control->dataBroker->unregisterTimedReceiver(this, "*", "*",
                                                   "mars_sim/simTimer");

      if(control->graphics) {
        if(cam_id) {
          control->graphics->removeHUDElement(cam_id);
        }
        if(cam_window_id) {
          control->graphics->remove3DWindow(cam_window_id);
        }
        control->graphics->removeGraphicsUpdateInterface(this);
      }
    }

    void CameraSensor::getCameraInfo(cameraStruct* cs)
    {
      if( gc )
        gc->getCameraInfo( cs );
      else
        std::cerr << "could not get camera info." << std::endl;
    }

    void CameraSensor::getImage(std::vector< Pixel >& buffer)
    {
        assert(buffer.size() == (config.width * config.height));
        int width;
        int height;
        gw->getImageData(reinterpret_cast<char *>(buffer.data()), width, height);

        assert(config.width == width);
        assert(config.height == height);
    }

    void CameraSensor::getDepthImage(std::vector< mars::sim::DistanceMeasurement >& buffer)
    {
        assert(buffer.size() == (config.width * config.height));
        int width;
        int height;
        gw->getRTTDepthData(reinterpret_cast<float *>(buffer.data()), width, height);

        assert(config.width == width);
        assert(config.height == height);
    }


    // this function is a hack currently, it uses sReal* as byte buffer
    // NOTE: never use the cameraSensor in a controller list!!!!
    int CameraSensor::getSensorData(sReal** data) const {
#warning "Work hiere"
#if 0
      if(gw) {
        // get image
        int width, height;
        if(s_cfg.depthImage) {
          // if there is a depth image, we would like to
          // to also get the normal image data.
          //
          // what is a problem, is that in order to have it
          // in the same block of memory, we need to copy
          // the individual results

          // get the image and depth data
          void *t1 = 0;
          gw->getImageData(&t1, width, height);
          size_t image_size = width * height * sizeof( uint8_t ) * 4;

          float *t2 = 0;
          gw->getRTTDepthData(&t2, width, height);
          size_t depth_size = width * height * sizeof( float );

          // get a new block of memory which will contain
          // both data
          size_t data_size = image_size + depth_size;
          uint8_t *res_mem = (uint8_t*)malloc( data_size );
          *data = (sReal*)res_mem;

          memcpy( res_mem, t1, image_size );
          memcpy( res_mem + image_size, t2, depth_size );

          free( t1 );
          free( t2 );

          return data_size;
        }
        else {
          gw->getImageData((void**)data, width, height);
          return width*height*4;
        }
      }
#endif
      return 0;
    }

    void CameraSensor::deactivateRendering() {
      if(config.enabled){
        control->graphics->deactivate3DWindow(cam_window_id);
        config.enabled = false;
      }

    }

    void CameraSensor::activateRendering() {
      if(!config.enabled){
        control->graphics->activate3DWindow(cam_window_id);
        config.enabled = true;
      }
    }

    void CameraSensor::preGraphicsUpdate(void) {
      mutex.lock();
      if(gc) {
        gc->updateViewportQuat(position.x(), position.y(), position.z(),
                               orientation.x(), orientation.y(),
                               orientation.z(), orientation.w());
        if(config.enabled) {
          if(renderCam > 2) --renderCam;
          else if(renderCam == 2) {
            control->graphics->activate3DWindow(cam_window_id);
            renderCam = 1;
          }
          else if(renderCam == 1) {
            control->graphics->deactivate3DWindow(cam_window_id);
            renderCam = 0;
          }
        }
      }
      mutex.unlock();
    }

    void CameraSensor::receiveData(const data_broker::DataInfo &info,
                                   const data_broker::DataPackage &package,
                                   int callbackParam) {
      CPP_UNUSED(info);
      mutex.lock();
      renderCam = 2+config.frameOffset;
      if(dbPosIndices[0] == -1) {
        dbPosIndices[0] = package.getIndexByName("position/x");
        dbPosIndices[1] = package.getIndexByName("position/y");
        dbPosIndices[2] = package.getIndexByName("position/z");
        dbRotIndices[0] = package.getIndexByName("rotation/x");
        dbRotIndices[1] = package.getIndexByName("rotation/y");
        dbRotIndices[2] = package.getIndexByName("rotation/z");
        dbRotIndices[3] = package.getIndexByName("rotation/w");
      }
      package.get(dbPosIndices[0], &position.x());
      package.get(dbPosIndices[1], &position.y());
      package.get(dbPosIndices[2], &position.z());
      package.get(dbRotIndices[0], &orientation.x());
      package.get(dbRotIndices[1], &orientation.y());
      package.get(dbRotIndices[2], &orientation.z());
      package.get(dbRotIndices[3], &orientation.w());
      position += (orientation * config.pos_offset);
      orientation= orientation * config.ori_offset;
      mutex.unlock();
    }


    BaseConfig* CameraSensor::parseConfig(ControlCenter *control,
                                          ConfigMap *config) {

      //ConfigMap *config = new ConfigMap();
      //(*config) = (*config_);

      CameraConfigStruct *cfg = new CameraConfigStruct();

      unsigned int mapIndex = (*config)["mapIndex"][0].getUInt();
      unsigned long attachedNodeID = (*config)["attached_node"][0].getULong();
      if(mapIndex) {
        attachedNodeID = control->loadCenter->getMappedID(attachedNodeID,
                                                          interfaces::MAP_TYPE_NODE,
                                                          mapIndex);
      }
      cfg->attached_node = attachedNodeID;

      ConfigMap::iterator it;
      ConfigMap::iterator it2;

      if((it = config->find("rate")) != config->end())
        cfg->updateRate = it->second[0].getULong();
      else cfg->updateRate = 100;

      if((it = config->find("frame_offset")) != config->end())
        cfg->frameOffset = it->second[0].getULong();
      else cfg->frameOffset = 1;

      if((it = config->find("width")) != config->end())
        cfg->width = it->second[0].getULong();

      if((it = config->find("height")) != config->end())
        cfg->height = it->second[0].getULong();

      if((it = config->find("opening_width")) != config->end()) // deprecated
        cfg->opening_width = it->second[0].getDouble();

      if((it = config->find("opening_angle")) != config->end())
        cfg->opening_width = it->second[0].getDouble();

      if((it = config->find("opening_height")) != config->end()) // deprecated
        cfg->opening_height = it->second[0].getDouble();

      if((it = config->find("opening_angle2")) != config->end())
        cfg->opening_height = it->second[0].getDouble();

      // correct opening_height
      if (cfg->opening_height < 0)
        cfg->opening_height = cfg->opening_width * ((double)cfg->height / (double)cfg->width);

      if((it = config->find("show_cam")) != config->end()){
        cfg->show_cam =  it->second[0].getBool();
        if(cfg->show_cam) {
          if((it = config->find("hud_idx")) != config->end())
            cfg->hud_pos = it->second[0].getInt();
        }
      }else{
        cfg->show_cam = false;
      }

      if((it = config->find("enabled")) != config->end()){
        cfg->enabled =  it->second[0].getBool();
      }else{
        cfg->enabled = true;
      }

      if((it = config->find("hud_size")) != config->end()) {
        cfg->hud_width = it->second[0].children["x"][0].getDouble();
        cfg->hud_height = it->second[0].children["y"][0].getDouble();
      }

      // if hud_height and hud_width are present, overwrite parameters
      if((it = config->find("hud_width")) != config->end())
        cfg->hud_width = it->second[0].getDouble();
      if((it = config->find("hud_height")) != config->end())
        cfg->hud_height = it->second[0].getDouble();
      else
        cfg->hud_height = cfg->hud_width * ((double)cfg->height / (double)cfg->width);

      if((it = config->find("position_offset")) != config->end()) {
        cfg->pos_offset[0] = it->second[0].children["x"][0].getDouble();
        cfg->pos_offset[1] = it->second[0].children["y"][0].getDouble();
        cfg->pos_offset[2] = it->second[0].children["z"][0].getDouble();
        LOG_DEBUG("camera position_offset: %g %g %g", cfg->pos_offset[0],
                  cfg->pos_offset[1], cfg->pos_offset[2]);
      }
      if((it = config->find("orientation_offset")) != config->end()) {
        if((it2 = it->second[0].children.find("yaw")) !=
           it->second[0].children.end()) {
          Vector euler;
          euler.x() = it->second[0].children["roll"][0].getDouble();
          euler.y() = it->second[0].children["pitch"][0].getDouble();
          euler.z() = it->second[0].children["yaw"][0].getDouble();
          cfg->ori_offset = eulerToQuaternion(euler);
        }
        else {
          cfg->ori_offset.x() = it->second[0].children["x"][0].getDouble();
          cfg->ori_offset.y() = it->second[0].children["y"][0].getDouble();
          cfg->ori_offset.z() = it->second[0].children["z"][0].getDouble();
          cfg->ori_offset.w() = it->second[0].children["w"][0].getDouble();
        }
      }

      return cfg;
    }

    ConfigMap CameraSensor::createConfig() const {

      std::vector<unsigned long>::const_iterator it;
      ConfigMap cfg;
      ConfigMap *tmpCfg;

      cfg["name"][0] = ConfigItem(config.name);
      cfg["id"][0] = ConfigItem(config.id);
      cfg["type"][0] = ConfigItem(std::string("CameraSensor"));

      cfg["attached_node"][0] = ConfigItem(config.attached_node);
      cfg["frame_offset"][0] = ConfigItem(config.frameOffset);

      cfg["width"][0] = ConfigItem(config.width);
      cfg["height"][0] = ConfigItem(config.height);
        
//      cfg["enabled"][0] = ConfigItem(config.enabled);


      if(config.show_cam) {
        cfg["show_cam"][0] = ConfigItem(true);
        cfg["show_cam"][0].children["hud_idx"][0] = ConfigItem(config.hud_pos);
      }

      cfg["position_offset"][0] = ConfigItem(std::string());
      tmpCfg = &(cfg["position_offset"][0].children);

      (*tmpCfg)["x"][0] = ConfigItem(config.pos_offset[0]);
      (*tmpCfg)["y"][0] = ConfigItem(config.pos_offset[1]);
      (*tmpCfg)["z"][0] = ConfigItem(config.pos_offset[2]);


      Quaternion q = eulerToQuaternion(Vector(90,0,-90));
      q.x() *= -1;
      q.y() *= -1;
      q.z() *= -1;
      q = config.ori_offset * q;

      cfg["orientation_offset"][0] = ConfigItem(std::string());
      tmpCfg = &(cfg["orientation_offset"][0].children);
      (*tmpCfg)["x"][0] = ConfigItem(q.x());
      (*tmpCfg)["y"][0] = ConfigItem(q.y());
      (*tmpCfg)["z"][0] = ConfigItem(q.z());
      (*tmpCfg)["w"][0] = ConfigItem(q.w());

      cfg["hud_size"][0] = ConfigItem(std::string());
      tmpCfg = &(cfg["hud_size"][0].children);
      (*tmpCfg)["x"][0] = ConfigItem(config.hud_width);
      (*tmpCfg)["y"][0] = ConfigItem(config.hud_height);

      return cfg;
    }

  } // end of namespace sim
} // end of namespace mars
