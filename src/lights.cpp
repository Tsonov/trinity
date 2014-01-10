#include "lights.h"
#include "random_generator.h"


int PointLight::getNumSamples()
{
	return 1;
}

void PointLight::getNthSample(int sampleIdx, const Vector& shadePos, Vector& samplePos, Color& color)
{
	samplePos = pos;
	color = this->color * this->power;
}

bool PointLight::intersect(const Ray& ray, double& intersectionDist)
{
	return false; // you can't intersect a point light
}


int RectLight::getNumSamples()
{
	return xSubd * ySubd;
}

void RectLight::getNthSample(int sampleIdx, const Vector& shadePos, Vector& samplePos, Color& color)
{
	Random& R = getRandomGen();
	
	// convert the shade point onto the light's canonic space:
	Vector shadePosCanonical = transform.undoPoint(shadePos);
	
	// shade point "behind" the lamp?
	if (shadePosCanonical.y > 0) {
		color.makeZero();
		return;
	}
	
	// stratified sampling:
	float sx = (sampleIdx % xSubd + R.randfloat()) / xSubd;
	float sy = (sampleIdx / xSubd + R.randfloat()) / ySubd;
	
	Vector sampleCanonical(sx - 0.5, 0, sy - 0.5);
	samplePos = transform.point(sampleCanonical);
	Vector shadePos_LS = shadePosCanonical - sampleCanonical;
	// return light color, attenuated by the angle of incidence
	// (the cosine between the light's direction and the normed ray toward the hitpos)
	color = this->color * (this->power * float(dot(Vector(0, -1, 0), shadePos_LS) / shadePos_LS.length()));
}

bool RectLight::intersect(const Ray& ray, double& intersectionDist)
{
	Ray ray_LS = transform.undoRay(ray);
	// check if ray_LS (the incoming ray, transformed in local space) hits the oriented square 1x1, resting
	// at (0, 0, 0), pointing downwards:
	if (ray_LS.start.y >= 0) return false; // ray start is in the wrong subspace; no intersection is possible
	if (ray_LS.dir.y <= 0) return false; // ray direction points downwards; no intersection is possible
	double lengthToIntersection = -(ray_LS.start.y / ray_LS.dir.y); // intersect with XZ plane
	Vector p = ray_LS.start + ray_LS.dir * lengthToIntersection;
	if (fabs(p.x) < 0.5 && fabs(p.z) < 0.5) {
		// the hit point is inside the 1x1 square - calculate the length to the intersection:
		double distance = (transform.point(p) - ray.start).length(); 
		
		if (distance < intersectionDist) {
			intersectionDist = distance;
			return true; // intersection found, and it improves the current closest dist
		}
	}
	return false;
}
