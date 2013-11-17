/***************************************************************************
 *   Copyright (C) 2009-2013 by Veselin Georgiev, Slavomir Kaslev et al    *
 *   admin@raytracing-bg.net                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <SDL/SDL.h>
#include <vector>
#include <iostream>
#include "sdl.h"
#include "matrix.h"
#include "camera.h"
#include "geometry.h"
#include "shading.h"
#include "environment.h"
using namespace std;

Color vfb[VFB_MAX_SIZE][VFB_MAX_SIZE]; //!< virtual framebuffer

Camera* camera;

vector<Geometry*> geometries;
vector<Shader*> shaders;
vector<Node*> nodes;
Environment* environment = NULL;

/// traces a ray in the scene and returns the visible light that comes from that direction
Color raytrace(Ray ray)
{
	IntersectionData data;
	Node* closestNode = NULL;
	
	if (ray.depth > MAX_TRACE_DEPTH) return Color(0, 0, 0);

	if (ray.debug)
		cout << "  Raytrace[start = " << ray.start << ", dir = " << ray.dir << "]\n";

	data.dist = 1e99;
	
	for (int i = 0; i < (int) nodes.size(); i++)
		if (nodes[i]->geom->intersect(ray, data))
			closestNode = nodes[i];

	if (!closestNode) {
		if (environment != NULL) return environment->getEnvironment(ray.dir);
		return Color(0, 0, 0);
	}
	
	if (ray.debug) {
		cout << "    Hit " << closestNode->geom->getName() << " at distance " << fixed << setprecision(2) << data.dist << endl;
		cout << "      Intersection point: " << data.p << endl;
		cout << "      Normal:             " << data.normal << endl;
		cout << "      UV coods:           " << data.u << ", " << data.v << endl;
	}
	
	return closestNode->shader->shade(ray, data);
}

/// checks for visibility between points `from' and `to'
/// (from is assumed to be near a surface, whereas to is near a light)
bool testVisibility(const Vector& from, const Vector& to)
{
	Ray ray;
	ray.start = from;
	ray.dir = to - from;
	ray.dir.normalize();
	
	IntersectionData temp;
	temp.dist = (to - from).length();
	
	for (int i = 0; i < (int) nodes.size(); i++)
		if (nodes[i]->geom->intersect(ray, temp))
			return false;
	
	return true;
}


void createNode(Geometry* geometry, Shader* shader)
{
	geometries.push_back(geometry);
	shaders.push_back(shader);
	Node* node = new Node(geometry, shader);
	nodes.push_back(node);
}

void initializeScene(void)
{
	camera = new Camera;
	camera->yaw = 5;
	camera->pitch = -5;
	camera->roll = 0;
	camera->fov = 60;
	camera->aspect = 4. / 3.0;
	camera->pos = Vector(45, 120, -300);
	
	camera->beginFrame();
	
	lightPos = Vector(-90, 1200, -750);
	lightColor = Color(1, 1, 1);
	lightPower = 1200000;
	ambientLight = Color(0.5, 0.5, 0.5);
	
	/* Create a floor node, with a layered shader: perfect reflection on top of woody diffuse */
	Plane* plane = new Plane(-0.01, 200);
	Texture* texture = new BitmapTexture("data/texture/wood.bmp", 0.0025);
	Lambert* lambert = new Lambert(Color(1, 1, 1), texture);
	Layered* planeShader = new Layered;
	planeShader->addLayer(lambert, Color(1, 1, 1));
	planeShader->addLayer(new Refl, Color(0.05, 0.05, 0.05), new Fresnel(1.33));
	createNode(plane, planeShader);

	Layered* glass = new Layered;
	glass->addLayer(new Refr(Color(0.97, 0.97, 0.97), 1.6), Color(1, 1, 1));
	glass->addLayer(new Refl(Color(0.97, 0.97, 0.97)), Color(1, 1, 1), new Fresnel(1.6));
	
	createNode(new Sphere(Vector(-60, 36, 10), 36), glass);

	/* Create a glossy sphere */
	Sphere* sphere = new Sphere(Vector(100, 50, 60), 50);
	Shader* glossy = new Refl(Color(0.9, 1.0, 0.9), 0.97, 25);
	
	createNode(sphere, glossy);
	
	Color colors[3] = { Color(1, 0, 0), Color(1, 1, 0), Color(0, 1, 0) };
	// desaturat a bit:
	for (int i = 0; i < 3; i++) colors[i].adjustSaturation(0.9f);
	for (int i = 0; i < 3; i++)
		createNode(new Sphere(Vector(10 + 32*i, 15, 0), 15), new Phong(colors[i]*0.75, 32));
		
	environment = new CubemapEnvironment("data/env/forest");
}

bool needsAA[VFB_MAX_SIZE][VFB_MAX_SIZE];

/// checks if two colors are "too different":
inline bool tooDifferent(const Color& a, const Color& b)
{
	const float THRESHOLD = 0.1;
	return (fabs(a.r - b.r) > THRESHOLD ||
		     fabs(a.g - b.g) > THRESHOLD ||
		     fabs(a.b - b.b) > THRESHOLD);
}

void renderScene(void)
{
	setWindowCaption("trinity: rendering");
	const double kernel[5][2] = {
		{ 0, 0 },
		{ 0.3, 0.3 },
		{ 0.6, 0 },
		{ 0, 0.6 },
		{ 0.6, 0.6 },
	};
	
	int W = frameWidth();
	int H = frameHeight();
	
	// first pass: shoot just one ray per pixel
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) 
			vfb[y][x] = raytrace(camera->getScreenRay(x, y));
		if (y % 10 == 0)
			displayVFB(vfb);
	}

	// second pass: find pixels, that need anti-aliasing, by analyzing their neighbours
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			Color neighs[5];
			neighs[0] = vfb[y][x];
			
			neighs[1] = vfb[y][x     > 0 ? x - 1 : x];
			neighs[2] = vfb[y][x + 1 < W ? x + 1 : x];

			neighs[3] = vfb[y     > 0 ? y - 1 : y][x];
			neighs[4] = vfb[y + 1 < H ? y + 1 : y][x];
			
			Color average(0, 0, 0);
			
			for (int i = 0; i < 5; i++)
				average += neighs[i];
			average /= 5.0f;
			
			for (int i = 0; i < 5; i++) {
				if (tooDifferent(neighs[i], average)) {
					needsAA[y][x] = true;
					break;
				}
			}
		}
	}

	bool previewAA = false; // change to true to make it just display which pixels are selected for anti-aliasing
	
	if (previewAA) {
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
				if (needsAA[y][x])
					vfb[y][x] = Color(1, 0, 0);
	} else {
		/* 
		 * A third pass, shooting additional rays for pixels that need them.
		 * Note that all pixels already are sampled with a ray at offset (0, 0),
		 * which coincides with sample #0 of our antialiasing kernel. So, instead
		 * of shooting five rays (the kernel size), we can just shoot the remaining
		 * four rays, adding with what we currently have in the pixel, and average
		 * after that.
		 */
		for (int y = 0; y < frameHeight(); y++) {
			for (int x = 0; x < frameWidth(); x++) { 
				if (needsAA[y][x]) {
					Color result = vfb[y][x]; // current result, identical to what we will
					                          // get if we shoot a new ray with i == 0 in the
					                          // following code:
					for (int i = 1; i < 5; i++)
						result += raytrace(camera->getScreenRay(x + kernel[i][0], y + kernel[i][1]));
					vfb[y][x] = result / 5.0f;
				}
			}
			if (y % 10 == 0)
				displayVFB(vfb);
		}
	}
}

/// handle a mouse click
void handleMouse(SDL_MouseButtonEvent *mev)
{
	if (mev->button != 1) return; // only consider the left mouse button
	printf("Mouse click from (%d, %d)\n", (int) mev->x, (int) mev->y);
	Ray ray = camera->getScreenRay(mev->x, mev->y);
	ray.debug = true;
	raytrace(ray);
	printf("Raytracing completed!\n");
}

int main(int argc, char** argv)
{
	if (!initGraphics(RESX, RESY)) return -1;
	initializeScene();
	Uint32 startTicks = SDL_GetTicks();
	renderScene();
	double renderTime = (SDL_GetTicks() - startTicks) / 1000.0;
	printf("Render time: %.2lf seconds.\n", renderTime);
	setWindowCaption("trinity: rendertime: %.2lfs", renderTime);
	displayVFB(vfb);
	waitForUserExit();
	closeGraphics();
	return 0;
}
