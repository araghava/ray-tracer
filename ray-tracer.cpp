// Copyright 2013 Sean McKenna
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

// a ray tracer in C++


// libraries, namespace
#include <thread>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include "library/loadXML.cpp"
#include "library/scene.cpp"
using namespace std;


// scene to load (project #) & debug options
string xml = "scenes/prj7.xml";
bool printXML = false;
bool zBuffer = false;
bool sampleCount = false;
int bounceCount = 5;


// variables for ray tracing
int w;
int h;
int size;
Color24* img;
float* zImg;
float* sampleImg;


// setup threading
static const int numThreads = 8;
void rayTracing(int i);


// for camera ray generation
void cameraRayVars();
float imageDistance = 1.0;
Point *imageTopLeftV;
Point *dXV;
Point *dYV;
Point firstPixel;
Transformation* c;
Point cameraRay(int pX, int pY);


// ray tracer
int main(){
  
  // load scene: root node, camera, image
  loadScene(xml, printXML);
  
  // set the scene as the root node
  setScene(rootNode);
  
  // set variables for ray tracing
  w = render.getWidth();
  h = render.getHeight();
  size = render.getSize();
  img = render.getRender();
  zImg = render.getZBuffer();
  
  // set variables for generating camera rays
  cameraRayVars();
  
  // start ray tracing loop (in parallel with threads)
  thread t[numThreads];
  for(int i = 0; i < numThreads; i++)
    t[i] = thread(rayTracing, i);
  
  // when finished, join all threads back to main
  for(int i = 0; i < numThreads; i++)
    t[i].join();
  
  // output ray-traced image & z-buffer (if set)
  render.save("images/image.ppm");
  if(zBuffer){
    render.computeZImage();
    render.saveZImage("images/imageZ.ppm");
  }
}


// ray tracing loop (for an individual pixel)
void rayTracing(int i){
  
  // initial starting pixel
  int pixel = i;
   
  // thread continuation condition
  while(pixel < size){
    
    // establish pixel location
    int pX = pixel % w;
    int pY = pixel / w;
    
    // transform ray into world space
    Point rayDir = cameraRay(pX, pY);
    Cone *ray = new Cone();
    ray->pos = camera.pos;
    ray->dir = c->transformFrom(rayDir);
    ray->radius = 0.0;
    ray->tan = dXV->x / (2.0 * imageDistance);
    
    // traverse through scene DOM
    // transform rays into model space
    // detect ray intersections and get back HitInfo
    HitInfo hi = HitInfo();
    bool hit = traceRay(*ray, hi);
    
    // update z-buffer, if necessary
    if(zBuffer)
      zImg[pixel] = hi.z;
    
    // color for the pixel
    Color24 c;
    
    // if hit, get the node's material
    if(hit){
      Node *n = hi.node;
      Material *m;
      if(n)
        m = n->getMaterial();
      
      // if there is a material, shade the pixel
      // 16-passes for reflections and refractions
      if(m)
        c = Color24(m->shade(*ray, hi, lights, bounceCount));
      
      // otherwise color it white (as a hit)
      else
        c.Set(237, 237, 237);
    
    // if we hit nothing, draw the background
    }else{
      Point p = Point((float) pX / w, (float) pY / h, 0.0);
      Color b = background.sample(p);
      c = b;
    }
    
    // color the pixel image
    img[pixel] = c;
    
    // re-assign next pixel (naive, but works)
    pixel += numThreads;
  }
}


// create variables for camera ray generation
void cameraRayVars(){
  float fov = camera.fov * M_PI / 180.0;
  float aspectRatio = (float) w / (float) h;
  float imageTipY = imageDistance * tan(fov / 2.0);
  float imageTipX = imageTipY * aspectRatio;
  float dX = (2.0 * imageTipX) / (float) w;
  float dY = (2.0 * imageTipY) / (float) h;
  imageTopLeftV = new Point(-imageTipX, imageTipY, -imageDistance);
  dXV = new Point(dX, 0.0, 0.0);
  dYV = new Point(0.0, -dY, 0.0);
  firstPixel = *imageTopLeftV + (*dXV * 0.5) + (*dYV * 0.5);
  
  // set up camera transformation (only need to rotate coordinates)
  c = new Transformation();
  Matrix *rotate = new cyMatrix3f();
  rotate->Set(camera.cross, camera.up, -camera.dir);
  c->transform(*rotate);
}


// compute camera rays
Point cameraRay(int pX, int pY){
  Point ray = firstPixel + (*dXV * pX) + (*dYV * pY);
  ray.Normalize();
  return ray;
}
