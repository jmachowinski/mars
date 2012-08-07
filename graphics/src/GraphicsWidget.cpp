/*
 *  Copyright 2011, 2012 DFKI GmbH Robotics Innovation Center
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

#include "QtOsgMixGraphicsWidget.h"
#include "GraphicsWidget.h"
#include "HUD.h"

#include <mars/utils/Color.h>

#include <iostream>
#include <string>

#include <osgViewer/ViewerEventHandlers>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/TerrainManipulator>
#include <osgWidget/Frame>

#define CULL_LAYER (1 << (widgetID-1))


namespace mars {
  namespace graphics {

    using namespace std;
    using namespace interfaces;

    template<class T>
    class map_data_compare : public std::binary_function<typename T::value_type, 
                                                         typename T::mapped_type, 
                                                         bool>
    {
    public:
      bool operator() (typename T::value_type &pair, 
                       typename T::mapped_type i) const
      {
        return pair.second == i;
      }
    };


    GraphicsWidget::GraphicsWidget(void *parent,
                                   osg::Group* scene, unsigned long id,
                                   bool isRTTWidget, int f):_osgWidgetWindowManager(0),_osgWidgetWindowCnt(0)
    {
      (void)f;
      (void)parent;

      widgetID = id;

      this->isRTTWidget = isRTTWidget;
      isStereoDisplay = isFullscreen = false;
      isMouseMoving = isMouseButtonDown = false;
      isHUDShown = true;

      graphicsWindow = 0;
      myHUD = 0;
      hudCamera = 0;
      graphicsCamera = 0;
      graphicsEventHandler = NULL;

      cameraEyeSeparation = 0.1;
      mouseX = mouseY = 0;
      pickmode = DISABLED;

      this->scene = scene;
      view = new osgViewer::View;
    }

    GraphicsWidget::~GraphicsWidget() {
      delete graphicsCamera;
    }

    int GraphicsWidget::addOsgWindow(osgWidget::Window* wnd){
      this->_osgWidgetWindowCnt++;
      int id= _osgWidgetWindowCnt;
  
      osg::ref_ptr<osgWidget::Window> w = getWindowById(id);
      if(w.valid()){
        std::cout<<"id is already in MAP !! error!!\n";
        return -1;
      }else {
        _osgWindowIdMap.insert(WindowIdMapType::value_type(id,wnd )); 
        std::cout<<"added window to the map:"<<id<<"\n";
      }
  
      return id;
    }


    bool GraphicsWidget::setFont(int id,const std::string &fontname){
      osg::ref_ptr<osgWidget::Widget> wnd = getWidgetById(id);
      if(wnd.valid()){
        osgWidget::Label* label = dynamic_cast<osgWidget::Label*>(wnd.get());
        if(label){
          label->setFont(fontname);
          return true;
        }
      }
      return false;
    }

    bool GraphicsWidget::setFontColor(int id,float r, float g,float b,float a){
      osg::ref_ptr<osgWidget::Widget> wnd = getWidgetById(id);
      if(wnd.valid()){
        osgWidget::Label* label = dynamic_cast<osgWidget::Label*>(wnd.get());
        if(label){
          label->setFontColor(r,g,b,a);
          return true;
        }
      }
      return false;
    }

    bool GraphicsWidget::setFontSize(int id,int size){
      osg::ref_ptr<osgWidget::Widget> wnd = getWidgetById(id);
      if(wnd.valid()){
        osgWidget::Label* label = dynamic_cast<osgWidget::Label*>(wnd.get());
        if(label){
          label->setFontSize(size);
          return true;
        }
      }
      return false;
    }
 
    bool GraphicsWidget::createStyle(const std::string& name,const std::string &style){
  
      osg::ref_ptr<osgWidget::WindowManager> wm = getOrCreateWindowManager();
      if(wm.valid()){
        std::cout<<"GraphicsWidget::createStyle:"<<style<<"\n";
        return  wm->getStyleManager()->addStyle(new osgWidget::Style(name, style));
      }
  
      return false;
   
    }
    bool GraphicsWidget::setSize(int id,float x, float y){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          wd->setSize(x,y);
          return true;
        }
      }else{
        wnd->resize(x,y);
        return true;
      }
      return false;
  
    }

  
    bool GraphicsWidget::setStyle(int id,const std::string &styleName){
      std::cerr<<" GraphicsWidget::setStyle"<<styleName<<"\n";
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          std::cerr<<" GraphicsWidget::setStyle SET IT !"<<"\n";
          wd->setStyle(styleName);
          return true;
        }
      }else{
        std::cerr<<" GraphicsWidget::setStyle SET IT !"<<"\n";
        wnd->setStyle(styleName);
        return true;
      }
      return false;
    }
  
    bool GraphicsWidget::hideWindow(int wndId){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(wndId);
      osg::ref_ptr<osgWidget::WindowManager> wm = getOrCreateWindowManager();
    
      if(wnd.valid() && wm.valid()){
        return wm->removeChild(wnd);
      }
  
      return false;
    }
    bool GraphicsWidget::deleteWidget(int wdgId)
    {
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(wdgId);
      if(wd.valid()){
        _osgWidgetIdMap.erase(wdgId);
        WidgetCallBackMapType::iterator it = _widgetCallBackMap.find(wdgId);
        if(it != _widgetCallBackMap.end()){
          _widgetCallBackMap.erase(it);
        }
      
        return true;
      }
    
      return false;
    }

    bool GraphicsWidget::deleteWindow(int wndId){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(wndId);
      osg::ref_ptr<osgWidget::WindowManager> wm = getOrCreateWindowManager();
      if(wnd.valid() && wm.valid()){
        wm->removeChild(wnd);
    
        const osgWidget::Window::Vector& v = wnd->getObjects();
        for(unsigned int i = 0;i<v.size();i++){
          osgWidget::Widget* w = v[i].get();
          WidgetIdMapType::iterator it = std::find_if( _osgWidgetIdMap.begin(), _osgWidgetIdMap.end(), std::bind2nd(map_data_compare<WidgetIdMapType>(), w) );

          if ( it != _osgWidgetIdMap.end() ){
            _osgWidgetIdMap.erase(it);
            std::cerr<<"erasing widget\n";
          }
      
        }
    
        _osgWindowIdMap.erase(wndId);
      }
  
      return false;
    }
    int GraphicsWidget::createInput(const std::string& name, const std::string& text, int count)
    {
      osg::ref_ptr<osgWidget::Label> widget =   new osgWidget::Input(name,text,count);        
      return addOsgWidget(widget);
    }

    int GraphicsWidget::createLabel(const std::string &name,const std::string &text){
   
      osg::ref_ptr<osgWidget::Label> widget =   new osgWidget::Label(name,text);        
      return addOsgWidget(widget);
    }

    bool GraphicsWidget::setAnchorVertical(int id,int va){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(wnd.valid()){
        wnd->setAnchorVertical( (osgWidget::Window::VerticalAnchor) (va+1) );
        return true;
      }
      return false;
    }

    bool GraphicsWidget::setAnchorHorizontal(int id,int ha){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(wnd.valid()){
        wnd->setAnchorHorizontal((osgWidget::Window::HorizontalAnchor) (ha+1));
        return true;
      }
      return false;
    }
  
    bool GraphicsWidget::setAlignHorizontal(int id,int h){
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->setAlignHorizontal((osgWidget::Widget::HorizontalAlignment)h);
        return true;
      }
      return false;
    }
    bool GraphicsWidget::setAlignVertical(int id, int v){
  
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->setAlignVertical( (osgWidget::Widget::VerticalAlignment) v);
        return true;
      }
      return false;
    }
    bool GraphicsWidget::getAlignHorizontal(int id,int &h){

      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        h = wd->getAlignHorizontal();
        return true;
      }
      return false;
    }
    bool GraphicsWidget::getAlignVertical(int id, int &v){

      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        v = wd->getAlignVertical();
        return true;
      }
      return false;
    }
  
    bool GraphicsWidget::getLayer(int id, int& layer)
    {
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        layer = wd->getLayer();
        return true;
      }
      return false;
    }
    bool GraphicsWidget::setLayer(int id, int layer, int offset)
    {
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->setLayer((osgWidget::Widget::Layer) layer,offset);
        return true;
      }
      return false;
    }



    bool GraphicsWidget::addWidgetToWindow(int window, int widget, int x, int y)
    {
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(window);
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(widget);
 
      if(wnd.valid() && wd.valid()){
        osgWidget::Table* table = dynamic_cast<osgWidget::Table*>(wnd.get());
        if(table){
          std::cout<<"added WIDGET TO TABLE.."<<x<<" "<<y<<"\n";
          table->addWidget(wd,x,y);
          return true;
        }else{
          osgWidget::Canvas* can = dynamic_cast<osgWidget::Canvas*>(wnd.get());
          if(can){
            std::cout<<"added WIDGET TO CANVAS..\n";
            can->addWidget(wd,x,y);
            return true;
          }else{
            osgWidget::Frame* frame = dynamic_cast<osgWidget::Frame*>(wnd.get());
            if(frame){
              osg::ref_ptr<osgWidget::Window> content = getWindowById(widget);
              if(content.valid()){
                frame->setWindow(content);
              }else{
                std::cerr<<"You spezified a Frame as Window, ";
                if(wd.valid()){
                  std::cerr<<"and a widget as content, but a Frame takes an other Window as content\n";
                }else{
                  std::cerr<<"but other Window as content is not valid\n";
                }
                
              }
           
            }else{
              std::cerr<<"UNKNOWN WINODW TYPE\n";
            }
          }
        }
        std::cout<<"wnd is not type canvas or !."<<std::endl;
      }
      return false;
    }

    bool GraphicsWidget::addWidgetToWindow(int window,int widget,float x, float y){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(window);
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(widget);
 
      if(wnd.valid() && wd.valid()){
        osgWidget::Canvas* can = dynamic_cast<osgWidget::Canvas*>(wnd.get());
        if(can){
          can->addWidget(wd,x,y);
          return true;
        }
        std::cout<<"wnd is not type canvas!."<<std::endl;
      }
      return false;
    }
  
    bool GraphicsWidget::addWidgetToWindow(int window, int widget)
    {
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(window);
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(widget);
 
      if(wnd.valid() && wd.valid()){
        wnd->addWidget(wd);
        std::cout<<"added widget to window."<<std::endl;
        return true;
      }
 
      std::cout<<"window or widget does not exist!\n";
      return false;
    }

    int GraphicsWidget::addOsgWidget(osgWidget::Widget *wid){
      this->_osgWidgetWindowCnt++;
      int id= _osgWidgetWindowCnt;
      osg::ref_ptr<osgWidget::Widget> wnd = getWidgetById(id);
      if(wnd.valid()){
        std::cout<<"id is already in MAP !! error!!\n";
        return -1;
      }else{
        _osgWidgetIdMap.insert(WidgetIdMapType::value_type(id,wid ));   
        std::cout<<"widget created."<<id<<std::endl;
      }
      return id;
    }
    int GraphicsWidget::createWidget(const std::string &name,float sizex,float sizey){
      osg::ref_ptr<osgWidget::Widget> widget =   new osgWidget::Widget(name,sizex,sizey);
      return addOsgWidget(widget.get());
    
    }
 
    bool GraphicsWidget::setColor(int id, float r, float g, float b, float a)
    {
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          wd->setColor(r,g,b,a);
          return true;
        }
      }else{
        wnd->getBackground()->setColor(r,g,b,a);
        return true;
      }
      return false;
    }

    osgWidget::Widget* GraphicsWidget::getWidgetById(int wdId){
      WidgetIdMapType::iterator iter= _osgWidgetIdMap.find(wdId);
      if(iter != _osgWidgetIdMap.end() ){
        return (*iter).second.get();
      }
      return NULL; 
    }
 
    osgWidget::Window* GraphicsWidget::getWindowById(int wndId){
      WindowIdMapType::iterator iter= _osgWindowIdMap.find(wndId);
      if(iter != _osgWindowIdMap.end() ){
        return (*iter).second.get();
      }
  
      return NULL;
    
    }
    bool GraphicsWidget::windowSetPosition(int wndId, float x, float y)
    {
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(wndId);
      if(wnd.valid()){
        wnd->setPosition(osgWidget::Point(x,y,0));
	return true;
      }
      return false;
    }

    bool GraphicsWidget::showWindow(int wndId){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(wndId);
      osg::ref_ptr<osgWidget::WindowManager> wm = getOrCreateWindowManager();
      if(wnd.valid() && wm.valid()){
        if(! wm->getByName(wnd->getName() )){// not already insered..
          wm->addChild(wnd);
        }
      }else{
        std::cout<<"window not known!\n";
        return false;
      }
  
      return true;
  
    }

    bool GraphicsWidget::setCanFill(int id, bool state){
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->setCanFill(state);
        return true;
      }
      return false;
    }
    bool GraphicsWidget::setShadow(int id,float intensity){
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        osgWidget::Label *label = dynamic_cast<osgWidget::Label*>( wd.get() );
        if(label){
          label->setShadow(intensity);
          return true;
        }
      }
      return false;
    }
    bool GraphicsWidget::addSize(int id, float x, float y){
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        wd->addSize(x,y);
        return true;
      }
      return false;
    }

    bool GraphicsWidget::addColor(int id,float r,float g,float b,float a){
      std::cerr<<"in add color\n";
        
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          wd->addColor(r,g,b,a);
          std::cerr<<"add color:r"<<r << " g:" << g<<" b:"<< b <<"\n";
          return true;
        }
      }else{
        wnd->getBackground()->addColor(r,g,b,a);
        return true;
      }
      return false;
    }
  
    bool GraphicsWidget::setLabel(int id, const std::string& text)
    {
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      if(wd.valid()){
        osgWidget::Label *label = dynamic_cast<osgWidget::Label*>( wd.get() );
        if(label){
          label->setLabel(text);
          return true;
        }
      }
      return false;
    }

    bool GraphicsWidget::manageClickEvent(osgWidget::Event& event)
    {
  
      std::cerr<<"\nseaching 4 callback!\n";
  
      WidgetCallBackPairType *ip =(WidgetCallBackPairType*) event.getData();
          
      if(ip){
        if(ip->second == NULL && ip->first) {
          guiClickCallBack  call=  ip->first;
          call(event.x ,event.y);
             
        }else{
          if(ip->second.get()) {
            (*(ip->second.get()))(event.x ,event.y);
          }
          std::cerr<<"base set\n"; 
        }
      }
   
      return false;
    }

    bool GraphicsWidget::setImage(int id, const std::string& path)
    {
      osg::ref_ptr<osg::Image> image = osgDB::readImageFile(path);
      if(!image.valid()){
        return false;
      }

      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);

      if(!wnd.valid()){
        osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
        if(wd.valid()){
          wd->setImage(image.get(),true);
          return true;
        }
      }else{
        wnd->getBackground()->setImage(image,true);
        return true;
      }
      return false;
    }



    bool GraphicsWidget::addEventToWidget(int id,guiClickCallBack function,guiClickCallBackBind *bindptr, osgWidget::EventType type){
      osg::ref_ptr<osgWidget::Window> wnd = getWindowById(id);
      osg::ref_ptr<osgWidget::Widget> wd = getWidgetById(id);
      
  
      if(wnd.valid() || wd.valid()){
        std::cout<<"found somethink to add callback \n";
        WidgetCallBackList *wcb;
        WidgetCallBackMapType::iterator it =  _widgetCallBackMap.find(id);
        if(it != _widgetCallBackMap.end()){
          wcb = &it->second;
        }else{
          _widgetCallBackMap.insert(WidgetCallBackMapType::value_type(id,WidgetCallBackList() ) );
          wcb = & _widgetCallBackMap.find(id)->second ;
        }
        wcb->push_back(WidgetCallBackPairType(function, std::tr1::shared_ptr<guiClickCallBackBind>(bindptr) ));
        if(wnd.valid()){
          wnd->setEventMask( wnd->getEventMask()| type);
          wnd->addCallback( new osgWidget::Callback(&GraphicsWidget::manageClickEvent,this, type,(void*)&wcb->back()));

        }else{
          std::cout<<"addCallback to Widget !! \n";
          wd->setEventMask(osgWidget::EVENT_ALL);      
          wd->addCallback( new osgWidget::Callback(&GraphicsWidget::manageClickEvent,this, type,(void*)&wcb->back()));

            
        }
        return true;
    
      }
      return false;
    }

    bool GraphicsWidget::addMousePushEventCallback(int id, guiClickCallBack function,guiClickCallBackBind *bindptr){
      return addEventToWidget(id,function,bindptr,osgWidget::EVENT_MOUSE_PUSH);
    }
    bool GraphicsWidget::addMouseReleaseEventCallback(int id, guiClickCallBack function,guiClickCallBackBind *bindptr){
      return addEventToWidget(id,function,bindptr,osgWidget::EVENT_MOUSE_RELEASE);
    }
    bool GraphicsWidget::addMouseEnterEventCallback(int id, GraphicsGuiInterface::guiClickCallBack function, guiClickCallBackBind* bindptr)
    {
      return addEventToWidget(id,function,bindptr,osgWidget::EVENT_MOUSE_ENTER);
    }
    bool GraphicsWidget::addMouseLeaveEventCallback(int id, guiClickCallBack function,guiClickCallBackBind *bindptr){
      return addEventToWidget(id,function,bindptr,osgWidget::EVENT_MOUSE_LEAVE);
    }
 
    int GraphicsWidget::createCanvas(const std::string& name)
    {
      std::cout<<"createCanvas"<<std::endl;
      osg::ref_ptr<osgWidget::Window>  wnd = new osgWidget::Canvas(name);
      return addOsgWindow(wnd);
    }
    int GraphicsWidget::createTable(const std::string& name, int row, int colums)
    {
      osg::ref_ptr<osgWidget::Window>  wnd = new osgWidget::Table(name,row,colums);
      return addOsgWindow(wnd);

    }

    int GraphicsWidget::createFrame(const std::string& name, float x1, float y1, float x2, float y2)
    {
      osg::ref_ptr<osgWidget::Frame> frame = osgWidget::Frame::createSimpleFrame(
                                                                                 name,
                                                                                 x1,
                                                                                 y1,
                                                                                 x2,
                                                                                 y2
                                                                                 );
      
      return addOsgWindow(frame);
    }

    int GraphicsWidget::createBox(const std::string& name,int type){
  
      std::cout<<"createBox"<<std::endl;
      osg::ref_ptr<osgWidget::Window>  wnd = new osgWidget::Box(name,type);
      return addOsgWindow(wnd);
    }
 
    osgWidget::WindowManager* GraphicsWidget::getOrCreateWindowManager()
    {
      if(this->_osgWidgetWindowManager){
        return _osgWidgetWindowManager;
      }
                
      osgWidget::point_type w = view->getCamera()->getViewport()->width();
      osgWidget::point_type h =  view->getCamera()->getViewport()->height();

              
      osgWidget::WindowManager* wm = new osgWidget::WindowManager(
                                                                  view,
                                                                  w,
                                                                  h,
                                                                  MASK_2D,
                                                                  osgWidget::WindowManager::WM_PICK_DEBUG| osgWidget::WindowManager::WM_USE_PYTHON
                                                                  );

      osg::Camera* camera = wm->createParentOrthoCamera();



      view->addEventHandler(new osgWidget::MouseHandler(wm));
      view->addEventHandler(new osgWidget::KeyboardHandler(wm));
      view->addEventHandler(new osgWidget::ResizeHandler(wm, camera));
      view->addEventHandler(new osgWidget::CameraSwitchHandler(wm, camera));
      view->addEventHandler(new osgViewer::StatsHandler());
      view->addEventHandler(new osgViewer::WindowSizeHandler());
      view->addEventHandler(new osgGA::StateSetManipulator(
                                                           view->getCamera()->getOrCreateStateSet()
                                                           ));

      view->getCamera()->addChild( camera );
      _osgWidgetWindowManager = wm;
    
      return wm;
  
    }

    void GraphicsWidget::initializeOSG(void* data,
                                       GraphicsWidget* shared, int width, int height) {
      osg::ref_ptr<osgGA::KeySwitchMatrixManipulator> keyswitchManipulator;

      if(width <= 0) width = height = 256;
      if(height <= 0) height = 256;

      // do not use osg default lighting
      view->setLightingMode(osg::View::NO_LIGHT);

      createContext(data, shared, width, height);
      if(!isRTTWidget) {
        graphicsWindow->setWindowName("3D Environment");

        keyswitchManipulator = new osgGA::KeySwitchMatrixManipulator;
        keyswitchManipulator->addMatrixManipulator('1',"Trackball",
                                                   new osgGA::TrackballManipulator() );
        keyswitchManipulator->addMatrixManipulator( '2', "Flight",
                                                    new osgGA::FlightManipulator() );
        //keyswitchManipulator->addMatrixManipulator( '3', "Drive",
        //    new osgGA::DriveManipulator() );
        keyswitchManipulator->addMatrixManipulator( '4', "Terrain",
                                                    new osgGA::TerrainManipulator() );

        initialize();
        view->addEventHandler(this);
        view->addEventHandler(keyswitchManipulator.get());
        view->addEventHandler(new osgViewer::StatsHandler);
      }
      view->setSceneData(scene);

      if(!isRTTWidget) graphicsCamera->setKeyswitchManipulator(keyswitchManipulator);
      graphicsCamera->changeCameraTypeToPerspective();
    }

    void GraphicsWidget::createContext(void* parent,
                                       GraphicsWidget* shared, int g_width, int g_height) {
      (void)parent;

      osg::ref_ptr<osg::Camera> osgCamera;

      if(!isRTTWidget) {
        osg::DisplaySettings* ds = osg::DisplaySettings::instance();
        osg::DisplaySettings* ds2 = new osg::DisplaySettings(*ds);
        osg::ref_ptr<osg::GraphicsContext::Traits> traits;
        traits = new osg::GraphicsContext::Traits;

        traits->readDISPLAY();
        if (traits->displayNum < 0)
          traits->displayNum = 0;
        traits->x = 20;
        traits->y = 50;
        widgetWidth = traits->width = 720;
        widgetHeight = traits->height = 405;
        traits->windowDecoration = true;
        traits->doubleBuffer = true;
        traits->alpha = ds->getMinimumNumAlphaBits();
        traits->stencil = ds->getMinimumNumStencilBits();
        traits->sampleBuffers = ds->getMultiSamples();
        traits->samples = ds->getNumMultiSamples();
        //traits->vsync = false;
        if (shared) {
          traits->sharedContext = shared->getGraphicsWindow();
        } else {
          traits->sharedContext = 0;
        }

        //fprintf(stderr, "\nsamples: %d\n", traits->sampleBuffers);

        osg::ref_ptr<osg::GraphicsContext> gc = createWidgetContext(parent, traits);

        graphicsWindow = dynamic_cast<osgViewer::GraphicsWindow*>(gc.get());
        osgCamera = new osg::Camera();
        osgCamera->setGraphicsContext(gc.get());
        osgCamera->setViewport(0, 0, 720, 405);
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        osgCamera->setDrawBuffer(buffer);
        osgCamera->setReadBuffer(buffer);
        osgCamera->setDisplaySettings(ds2);
        osgCamera->setCullMask(CULL_LAYER);
        view->setCamera(osgCamera.get());

        osg::Image* grabImage = new osg::Image();
        postDrawCallback = new PostDrawCallback(grabImage);
        postDrawCallback->setSize(widgetWidth, widgetHeight);
        postDrawCallback->setGrab(false);
        //osgCamera->setFinalDrawCallback(postDrawCallback);
      }
      else { // hasRTTWidget == true
        osgCamera = new osg::Camera();
        if(!shared) {
          osg::DisplaySettings* ds = osg::DisplaySettings::instance();
          osg::DisplaySettings* ds2 = new osg::DisplaySettings(*ds);
          (void)ds2;
          osg::ref_ptr<osg::GraphicsContext::Traits> traits;
          traits = new osg::GraphicsContext::Traits;
          traits->alpha = ds->getMinimumNumAlphaBits();
          traits->stencil = ds->getMinimumNumStencilBits();
          traits->windowDecoration = true;
          traits->sampleBuffers = ds->getMultiSamples();
          traits->samples = ds->getNumMultiSamples();
          traits->x = 0;
          traits->y = 0;
          widgetWidth = traits->width = g_width;
          widgetHeight = traits->height = g_height;
          traits->doubleBuffer = false;
          //traits->pbuffer = true;
          traits->windowDecoration = false;

          osg::ref_ptr<osg::GraphicsContext> gc;
          gc = osg::GraphicsContext::createGraphicsContext(traits.get());
          osgCamera->setGraphicsContext(gc.get());
        }
        else {
          osgCamera->setGraphicsContext(shared->getGraphicsWindow());
        }

        osg::DisplaySettings* ds = osg::DisplaySettings::instance();
        osg::DisplaySettings* ds2 = new osg::DisplaySettings(*ds);
        osgCamera->setDisplaySettings(ds2);
        osgCamera->setViewport(0, 0, g_width, g_height);
        view->setCamera(osgCamera.get());

        osgCamera->setRenderOrder(osg::Camera::PRE_RENDER);
        osgCamera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        osgCamera->setAllowEventFocus(false);
        osgCamera->setCullMask(CULL_LAYER);
        rttTexture = new osg::Texture2D();
        rttTexture->setDataVariance(osg::Object::DYNAMIC);
        rttTexture->setTextureSize(g_width, g_height);
        rttTexture->setInternalFormat(GL_RGBA);
        rttTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        rttTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
        rttTexture->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR);
        rttTexture->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::LINEAR);
    

    
        rttImage = new osg::Image();
        rttImage->allocateImage(g_width, g_height,
                                1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
        osgCamera->attach(osg::Camera::COLOR_BUFFER, rttImage.get());
        rttTexture->setImage(rttImage);

        // depth component
        rttDepthTexture = new osg::Texture2D();
        rttDepthTexture->setDataVariance(osg::Object::DYNAMIC);
        rttDepthTexture->setTextureSize(g_width, g_height);
        rttDepthTexture->setInternalFormat(GL_FLOAT);
        rttDepthTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        rttDepthTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
        rttDepthTexture->setFilter(osg::Texture2D::MIN_FILTER,
                                   osg::Texture2D::LINEAR);
        rttDepthTexture->setFilter(osg::Texture2D::MAG_FILTER,
                                   osg::Texture2D::LINEAR);
        rttDepthImage = new osg::Image();
        rttDepthImage->allocateImage(g_width, g_height,
                                     1, GL_DEPTH_COMPONENT, GL_FLOAT);

#ifdef DEPTH_IMAGES //If Depth images are enabled it crashes on some machines simply later 
        osgCamera->attach(osg::Camera::DEPTH_BUFFER, rttDepthImage.get());
#else
        osgCamera->attach(osg::Camera::COLOR_BUFFER, rttTexture.get(), 0, 0, false, 2, 0);
#endif


        rttDepthTexture->setImage(rttDepthImage);
      }
      graphicsCamera = new GraphicsCamera(osgCamera, g_width, g_height);
    }

    unsigned long GraphicsWidget::getID(void) {
      return widgetID;
    }

    osgViewer::View* GraphicsWidget::getView() {
      return view;
    }

    void GraphicsWidget::updateView(void) {
      graphicsCamera->update();
    }

    osgViewer::GraphicsWindow* GraphicsWidget::getGraphicsWindow() {
      return graphicsWindow.get();
    }
    const osgViewer::GraphicsWindow* GraphicsWidget::getGraphicsWindow() const {
      return graphicsWindow.get();
    }

    osg::ref_ptr<osg::Camera> GraphicsWidget::getMainCamera(){
      return graphicsCamera->getOSGCamera();
    }

    mars::utils::Vector GraphicsWidget::getMousePos(){
      return mars::utils::Vector(mouseX, mouseY, 0.0);
    }

    void GraphicsWidget::setFullscreen(bool val, int display) {
      if (!isFullscreen && val) {
        osg::GraphicsContext::WindowingSystemInterface *wsi =
          osg::GraphicsContext::getWindowingSystemInterface();
        unsigned int screenWidth = 1024;
        unsigned int screenHeight = 768;

        if (wsi != NULL) {
          wsi->getScreenResolution(*(graphicsWindow->getTraits()), screenWidth, screenHeight);
        }

        setWidgetFullscreen(true);
        isFullscreen = true;
        graphicsWindow->useCursor(false);
      } else if(isFullscreen && !val) {
        setWidgetFullscreen(false);
        isFullscreen = false;
        graphicsWindow->useCursor(true);
      }
    }

    void GraphicsWidget::setGraphicsEventHandler(GraphicsEventInterface* graphicsEventHandler) {
      this->graphicsEventHandler = graphicsEventHandler;
    }

    GraphicsCameraInterface* GraphicsWidget::getCameraInterface(void) const {
      assert(graphicsCamera);
      return dynamic_cast<GraphicsCameraInterface*>(graphicsCamera);
    }


    void GraphicsWidget::setHUD(HUD* theHUD) {
      myHUD = theHUD;
      theHUD->resize(widgetWidth, widgetHeight);
      theHUD->setCullMask(CULL_LAYER);
      theHUD->getCamera()->setFinalDrawCallback(postDrawCallback);
      //view->addSlave(theHUD->getCamera(), false);
      view->getCamera()->addChild( theHUD->getCamera() );
    }

    void GraphicsWidget::addHUDElement(HUDElement* elem) {
      if(myHUD) myHUD->addHUDElement(elem);
    }

    void GraphicsWidget::switchHudElemtVis(int num_element) {
      if(myHUD) myHUD->switchElementVis(num_element);
    }


    osg::Texture2D* GraphicsWidget::getRTTTexture(void) {
      return rttTexture.get();
    }

    osg::Texture2D* GraphicsWidget::getRTTDepthTexture(void) {
      return rttDepthTexture.get();
    }

    void GraphicsWidget::clearSelectionVectors() {
      pickedObjects.clear();
    }

    void GraphicsWidget::setGrabFrames(bool grab) {
      if(!isRTTWidget) postDrawCallback->setGrab(grab);
    }

    void GraphicsWidget::setSaveFrames(bool grab) {
      if(!isRTTWidget) postDrawCallback->setSaveGrab(grab);
    }

    std::vector<osg::Node*> GraphicsWidget::getPickedObjects() {
      return pickedObjects;
    }

    void GraphicsWidget::setClearColor(mars::utils::Color color){
      graphicsCamera->getOSGCamera()->setClearColor(
                                                    osg::Vec4(color.r, color.g, color.b, color.a));
    }

    void GraphicsWidget::grabFocus() {
      getGraphicsWindow()->grabFocus();
    }

    void GraphicsWidget::getImageData(void **data, int &width, int &height, bool depthImage) {
      if(isRTTWidget) {
        osg::Image *image = depthImage?rttDepthImage:rttImage;
        width = image->s();
        height = image->t();
        *data = malloc(width*height*4);
        memcpy(*data, image->data(), width*height*4);
      }
      else {
        postDrawCallback->getImageData(data, width, height);
      }
    }

    void GraphicsWidget::getRTTDepthData(float **data, int &width, int &height) {
      if(isRTTWidget) {
        float* data2 = (float*)rttDepthImage->data();
        width = rttDepthImage->s();
        height = rttDepthImage->t();
        *data = (float*)malloc(width*height*sizeof(float));

        double fovy, aspectRatio, Zn, Zf;
        graphicsCamera->getOSGCamera()->getProjectionMatrixAsPerspective( fovy, aspectRatio, Zn, Zf );
        int d = 0;
        for(int i=height-1; i>=0; --i) {
          for(int k=0; k<width; ++k) {
            (*data)[d++] = Zn*Zf/(Zf-data2[i*width+k]*(Zf-Zn));
          }
        }
      }
    }

    bool GraphicsWidget::handle(
                                const osgGA::GUIEventAdapter& ea,
                                osgGA::GUIActionAdapter& aa)
    {
      
      // remember position for mouse/camera interaction
      mouseX = ea.getX();
      mouseY = ea.getY();

      // wrap events to class methods
      switch (ea.getEventType()) {
      case osgGA::GUIEventAdapter::KEYDOWN :
        return handleKeyDownEvent(ea);
      case osgGA::GUIEventAdapter::KEYUP :
        if (graphicsEventHandler) {
          graphicsEventHandler->emitSetAppActive(widgetID);
        }
        sendKeyUpEvent(ea);
        return handleKeyUpEvent(ea);
      case osgGA::GUIEventAdapter::PUSH :
        return handlePushEvent(ea);
      case osgGA::GUIEventAdapter::DRAG :
        return handleDragEvent(ea);
      case osgGA::GUIEventAdapter::SCROLL:
        return handleScrollEvent(ea);
      case osgGA::GUIEventAdapter::RELEASE :
        return handleReleaseEvent(ea, aa);
      case osgGA::GUIEventAdapter::RESIZE :
        // the view does not receive release events for CTRL+D when the view is (un)docked
        // setting an empty event queue fixes his problem.
        view->setEventQueue(new osgGA::EventQueue);
        return handleResizeEvent(ea);
      case osgGA::GUIEventAdapter::FRAME :
        return false;
      case osgGA::GUIEventAdapter::QUIT_APPLICATION:
        if (graphicsEventHandler) graphicsEventHandler->emitQuitEvent(widgetID);
        return true;
      case osgGA::GUIEventAdapter::CLOSE_WINDOW:
        return false;
    
      default:
        return false;
      }
    }

    bool GraphicsWidget::handleResizeEvent(const osgGA::GUIEventAdapter& ea) {
      widgetWidth = ea.getWindowWidth();
      widgetHeight = ea.getWindowHeight();
      postDrawCallback->setSize(widgetWidth, widgetHeight);
      graphicsCamera->setViewport(0, 0, widgetWidth, widgetHeight);
      graphicsCamera->changeCameraTypeToPerspective();
      if (hudCamera) hudCamera->setViewport(0, 0, widgetWidth, widgetHeight);
      if (myHUD) myHUD->resize(widgetWidth, widgetHeight);

      if(graphicsEventHandler) {
        graphicsEventHandler->emitGeometryChange(widgetID,
                                                 ea.getWindowX(), ea.getWindowY(), widgetWidth, widgetHeight);
      }

      return true;
    }

    bool GraphicsWidget::handleReleaseEvent(const osgGA::GUIEventAdapter& ea,
                                            osgGA::GUIActionAdapter& aa) {
      // *** Picking ***

      isMouseButtonDown = false;
      isMouseMoving = false;

      if(pickmode == DISABLED) return false;

      osgViewer::View* view = dynamic_cast<osgViewer::View*>(&aa);
      if(!view) {
        cerr << "View cast failed!" << endl;
        return false;
      }

      // Mouse didn't move
      if(mouseX==ea.getX() || mouseY==ea.getY()) {
        if(pick(mouseX, mouseY)) {
          if(graphicsEventHandler) {
            graphicsEventHandler->emitNodeSelectionChange(widgetID, (int)this->pickmode);
            return true;
          }
          else if(!graphicsEventHandler) {
            cerr << "Error: object pick processing not possible : ";
            cerr << "no event handler found." << endl;
            return false;
          }
        }
      }

      return false;
    }

    bool GraphicsWidget::handleKeyDownEvent(const osgGA::GUIEventAdapter &ea) {
      switch (ea.getKey()) {
      case 'f' :
        return true;
      case osgGA::GUIEventAdapter::KEY_Escape :
        return true;
      case osgGA::GUIEventAdapter::KEY_Up:
        graphicsCamera->move(true, GraphicsCamera::FORWARD);
        return true;
      case osgGA::GUIEventAdapter::KEY_Down:
        graphicsCamera->move(true, GraphicsCamera::BACKWARD);
        return true;
      case osgGA::GUIEventAdapter::KEY_Left:
        graphicsCamera->move(true, GraphicsCamera::LEFT);
        return true;
      case osgGA::GUIEventAdapter::KEY_Right :
        graphicsCamera->move(true, GraphicsCamera::RIGHT);
        return true;
      default:
        return false;
      }
    }

    bool GraphicsWidget::handleKeyUpEvent(const osgGA::GUIEventAdapter &ea) {
      int key = ea.getKey();

      switch (key) {
      case osgGA::GUIEventAdapter::KEY_Escape :
        return true;
        break;
      case 'f' :
        setFullscreen(!isFullscreen);
        break;
      case 'h' :
        if (!isHUDShown) {
          isHUDShown = 1;
          if (myHUD) myHUD->setCullMask(CULL_LAYER);
        }
        else {
          isHUDShown = 0;
          if (myHUD) myHUD->setCullMask(0x0);
        }
        break;
      case osgGA::GUIEventAdapter::KEY_Up:
        graphicsCamera->move(false, GraphicsCamera::FORWARD);
        return true;
      case osgGA::GUIEventAdapter::KEY_Down:
        graphicsCamera->move(false, GraphicsCamera::BACKWARD);
        return true;
      case osgGA::GUIEventAdapter::KEY_Left:
        graphicsCamera->move(false, GraphicsCamera::LEFT);
        return true;
      case osgGA::GUIEventAdapter::KEY_Right :
        graphicsCamera->move(false, GraphicsCamera::RIGHT);
        return true;
      case '1' :
      case '2' :
      case '3' :
      case '4' :
      case '5' :
      case '6' :
      case '7' :
      case '8' :
      case '9' :
        if (myHUD) myHUD->switchCullElement(key);
        break;
      case '.' :
        graphicsCamera->toggleStereoMode();
        break;
      case '+' :
        cameraEyeSeparation += 0.02;
        graphicsCamera->setEyeSep(cameraEyeSeparation);
        graphicsCamera->getOSGCamera()->getDisplaySettings()->setEyeSeparation(cameraEyeSeparation/10.0);
        break;
      case '-' :
        cameraEyeSeparation -= 0.02;
        graphicsCamera->setEyeSep(cameraEyeSeparation);
        graphicsCamera->getOSGCamera()->getDisplaySettings()->setEyeSeparation(cameraEyeSeparation/10.0);
        break;
      case '#' :
        isStereoDisplay = !isStereoDisplay;
        graphicsCamera->getOSGCamera()->getDisplaySettings()->setStereo(isStereoDisplay);
        break;
      case osgGA::GUIEventAdapter::KEY_Control_L :
      case osgGA::GUIEventAdapter::KEY_Control_R :
        this->pickmode = DISABLED;
        break;
      default:
        return false;
      } // switch

      return false;
    }

    bool GraphicsWidget::handlePushEvent(const osgGA::GUIEventAdapter& ea) {

      unsigned int modKey = ea.getModKeyMask();

      // we already are in camera move mode and therefore
      // do not want to enter pick mode
      if(isMouseButtonDown == true) {
        return false;
      }
      // we are not in camera move mode and enter pick mode
      else if(modKey & osgGA::GUIEventAdapter::MODKEY_CTRL) {
        if(this->pickmode == DISABLED)
          this->pickmode = STANDARD;
      }
      // we are not in camera move mode and also do not want to activate pick mode
      else {
        graphicsCamera->eventStartPos((int)ea.getX(), (int)ea.getY());
      }
      isMouseButtonDown = true;
      return false;
    }

    bool GraphicsWidget::handleDragEvent(const osgGA::GUIEventAdapter &ea) {
      if (isMouseButtonDown && pickmode == DISABLED) {
        graphicsCamera->mouseDrag(ea.getButtonMask(),
                                  (int)ea.getX(), (int)ea.getY());
        isMouseMoving = true;
      }
      return false;
    }

    bool GraphicsWidget::handleScrollEvent(const osgGA::GUIEventAdapter& ea)
    {
      if(ea.getScrollingMotion()==osgGA::GUIEventAdapter::SCROLL_UP){
        graphicsCamera->zoom(1);
      }else if(ea.getScrollingMotion()==osgGA::GUIEventAdapter::SCROLL_DOWN){
        graphicsCamera->zoom(-1);
      }
      return false;
    }

    void GraphicsWidget::sendKeyUpEvent(const osgGA::GUIEventAdapter &ea) {
      if (graphicsEventHandler) {
        int key = ea.getKey();
        unsigned int modKey = ea.getModKeyMask();
        unsigned int mod = 0;

        if (1 <= key && key <= 26) {
          key += 64;
        } else if (97 <= key && key <= 122) {
          key -= 32;
        } else if (key >= 0xFF00) {
          translateKey(key, mod);
        }

        if (modKey & osgGA::GUIEventAdapter::MODKEY_SHIFT) {
          mod |= GuiEventInterface::ShiftModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_CTRL) {
          mod |= GuiEventInterface::ControlModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_ALT) {
          mod |= GuiEventInterface::AltModifier;
        }
        if (modKey & osgGA::GUIEventAdapter::MODKEY_META) {
          mod |= GuiEventInterface::MetaModifier;
        }
        graphicsEventHandler->emitKeyUpEvent(key, mod, widgetID);
      }
    }

    void GraphicsWidget::translateKey(int &key, unsigned int &mod) {
      switch (key) {
      case osgGA::GUIEventAdapter::KEY_BackSpace :
        key = GuiEventInterface::Key_Backspace;
        break;
      case osgGA::GUIEventAdapter::KEY_Tab :
        key = GuiEventInterface::Key_Tab;
        break;
      case osgGA::GUIEventAdapter::KEY_Return :
        key = GuiEventInterface::Key_Return;
        break;
      case osgGA::GUIEventAdapter::KEY_Pause :
        key = GuiEventInterface::Key_Pause;
        break;
      case osgGA::GUIEventAdapter::KEY_Scroll_Lock :
        key = GuiEventInterface::Key_ScrollLock;
        break;
      case osgGA::GUIEventAdapter::KEY_Escape :
        key = GuiEventInterface::Key_Escape;
        break;
      case osgGA::GUIEventAdapter::KEY_Delete :
        key = GuiEventInterface::Key_Delete;
        break;
      case osgGA::GUIEventAdapter::KEY_Home :
        key = GuiEventInterface::Key_Home;
        break;
      case osgGA::GUIEventAdapter::KEY_Left :
        key = GuiEventInterface::Key_Left;
        break;
      case osgGA::GUIEventAdapter::KEY_Up :
        key = GuiEventInterface::Key_Up;
        break;
      case osgGA::GUIEventAdapter::KEY_Right :
        key = GuiEventInterface::Key_Right;
        break;
      case osgGA::GUIEventAdapter::KEY_Down :
        key = GuiEventInterface::Key_Down;
        break;
      case osgGA::GUIEventAdapter::KEY_Page_Up :
        key = GuiEventInterface::Key_PageUp;
        break;
      case osgGA::GUIEventAdapter::KEY_Page_Down :
        key = GuiEventInterface::Key_PageDown;
        break;
      case osgGA::GUIEventAdapter::KEY_End :
        key = GuiEventInterface::Key_End;
        break;
      case osgGA::GUIEventAdapter::KEY_Print :
        key = GuiEventInterface::Key_Print;
        break;
      case osgGA::GUIEventAdapter::KEY_Insert :
        key = GuiEventInterface::Key_Insert;
        break;
      case osgGA::GUIEventAdapter::KEY_Num_Lock :
        key = GuiEventInterface::Key_NumLock;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Space :
        key = GuiEventInterface::Key_Space;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Tab :
        key = GuiEventInterface::Key_Tab;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Enter :
        key = GuiEventInterface::Key_Enter;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_F1 :
        key = GuiEventInterface::Key_F1;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_F2 :
        key = GuiEventInterface::Key_F2;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_F3 :
        key = GuiEventInterface::Key_F3;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_F4 :
        key = GuiEventInterface::Key_F4;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Home :
        key = GuiEventInterface::Key_Home;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Left :
        key = GuiEventInterface::Key_Left;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Up :
        key = GuiEventInterface::Key_Up;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Right :
        key = GuiEventInterface::Key_Right;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Down :
        key = GuiEventInterface::Key_Down;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Page_Up :
        key = GuiEventInterface::Key_PageUp;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Page_Down :
        key = GuiEventInterface::Key_PageDown;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_End :
        key = GuiEventInterface::Key_End;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Insert :
        key = GuiEventInterface::Key_Insert;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Delete :
        key = GuiEventInterface::Key_Delete;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Equal :
        key = GuiEventInterface::Key_Equal;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Multiply :
        key = GuiEventInterface::Key_multiply;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Add :
        key = GuiEventInterface::Key_Plus;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Subtract :
        key = GuiEventInterface::Key_Minus;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_Divide :
        key = GuiEventInterface::Key_Slash;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_0 :
        key = GuiEventInterface::Key_0;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_1 :
        key = GuiEventInterface::Key_1;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_2 :
        key = GuiEventInterface::Key_2;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_3 :
        key = GuiEventInterface::Key_3;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_4 :
        key = GuiEventInterface::Key_4;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_5 :
        key = GuiEventInterface::Key_5;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_6 :
        key = GuiEventInterface::Key_6;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_7 :
        key = GuiEventInterface::Key_7;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_8 :
        key = GuiEventInterface::Key_8;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_KP_9 :
        key = GuiEventInterface::Key_9;
        mod |= GuiEventInterface::KeypadModifier;
        break;
      case osgGA::GUIEventAdapter::KEY_F1 :
        key = GuiEventInterface::Key_F1;
        break;
      case osgGA::GUIEventAdapter::KEY_F2 :
        key = GuiEventInterface::Key_F2;
        break;
      case osgGA::GUIEventAdapter::KEY_F3 :
        key = GuiEventInterface::Key_F3;
        break;
      case osgGA::GUIEventAdapter::KEY_F4 :
        key = GuiEventInterface::Key_F4;
        break;
      case osgGA::GUIEventAdapter::KEY_F5 :
        key = GuiEventInterface::Key_F5;
        break;
      case osgGA::GUIEventAdapter::KEY_F6 :
        key = GuiEventInterface::Key_F6;
        break;
      case osgGA::GUIEventAdapter::KEY_F7 :
        key = GuiEventInterface::Key_F7;
        break;
      case osgGA::GUIEventAdapter::KEY_F8 :
        key = GuiEventInterface::Key_F8;
        break;
      case osgGA::GUIEventAdapter::KEY_F9 :
        key = GuiEventInterface::Key_F9;
        break;
      case osgGA::GUIEventAdapter::KEY_F10 :
        key = GuiEventInterface::Key_F10;
        break;
      case osgGA::GUIEventAdapter::KEY_F11 :
        key = GuiEventInterface::Key_F11;
        break;
      case osgGA::GUIEventAdapter::KEY_F12 :
        key = GuiEventInterface::Key_F12;
        break;
      }
    }

    bool GraphicsWidget::pick(const double x, const double y) {
      osgUtil::LineSegmentIntersector::Intersections intersections;
      osgUtil::LineSegmentIntersector::Intersections::iterator hitr;
      osg::PositionAttitudeTransform* posTransform;
      osg::Transform* transform;

#ifdef __linux__
      if(!view->computeIntersections(x, -y, intersections))
#else
        if(!view->computeIntersections(x, y, intersections))
#endif
          return false;

      // *** for each intersection we found ***
      for(hitr=intersections.begin(); hitr!=intersections.end(); ++hitr) {
        // choose foremost selection in FoV
        if(!(hitr==intersections.begin()) || !(!hitr->nodePath.empty()))
          continue;

        osg::NodePath nodePath = hitr->nodePath;
        unsigned int i = nodePath.size();
        while (i--) {
          transform = nodePath[i]->asTransform();
          if(!transform) continue;

          posTransform = transform->asPositionAttitudeTransform();
          if(!posTransform) continue;

          pickedObjects.push_back(posTransform);
          return true;
        }
      }

      return false;
    }

  } // end of namespace graphics
} // end of namespace mars