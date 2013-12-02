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
#ifndef __MESH_H__
#define __MESH_H__

#include <vector>
#include "vector.h"
#include "geometry.h"

/// A structure to represent a single triangle in the mesh
struct Triangle {
	int v[3]; //!< holds indices to the three vertices of the triangle (indexes in the `vertices' array in the Mesh)
	int n[3]; //!< holds indices to the three normals of the triangle (indexes in the `normals' array)
	int t[3]; //!< holds indices to the three texture coordinates of the triangle (indexes in the `uvs' array)
	Vector gnormal; //!< The geometric normal of the mesh (AB ^ AC, normalized)
	Vector dNdx, dNdy;
	
	Triangle(std::string a, std::string b, std::string c);
};

class Mesh: public Geometry {
	std::vector<Vector> vertices; //!< An array with all vertices in the mesh
	std::vector<Vector> normals; //!< An array with all normals in the mesh
	std::vector<Vector> uvs; //!< An array with all texture coordinates in the mesh
	std::vector<Triangle> triangles; //!< An array that holds all triangles
	
	// intersect a ray with a single triangle. Return true if an intersection exists, and it's
	// closer to the minimum distance, stored in data.dist
	bool intersectTriangle(const Ray& ray, IntersectionData& data, Triangle& T);
	void initMesh(void);
	
	bool faceted; //!< whether the normals interpolation is disabled or not
	bool backfaceCulling; //!< whether the backfaceCulling optimization is enabled (default: yes)
	bool hasNormals; //!< whether the .obj file contained normals. If not, no normal smoothing can be used.
	Sphere* boundingSphere; //!< a bounding sphere, which optimizes our whole
	
	bool loadFromOBJ(const char* filename); //!< load a mesh from an .OBJ file.
public:
	Mesh() { faceted = false; boundingSphere = NULL; backfaceCulling = true; }
	~Mesh();
	const char* getName();
	bool intersect(Ray ray, IntersectionData& info);
	bool isInside(const Vector& p) const { return false; } //FIXME!!
	
	void setFaceted(bool faceted) { this->faceted = faceted; }
	
	void fillProperties(ParsedBlock& pb)
	{
		char fileName[256];
		if (pb.getFilenameProp("file", fileName))
			loadFromOBJ(fileName);
		else
			pb.requiredProp("file");
		pb.getBoolProp("faceted", &faceted);
		pb.getBoolProp("backfaceCulling", &backfaceCulling);
		initMesh();
	}
};

#endif // __MESH_H__
