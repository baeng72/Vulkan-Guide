#pragma once
//based on https://vkguide.dev/ by Victor Blanco
#include "vk_types.h"
#include <vector>
#include <glm/vec3.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <iostream>

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
	glm::vec3	position;
	glm::vec3	normal;
	glm::vec3	color;
	static VertexInputDescription get_vertex_description() {
		VertexInputDescription description;
		//we will have just 1 vertex binding, with a per-vertex rate.
		VkVertexInputBindingDescription mainBinding;
		mainBinding.binding = 0;
		mainBinding.stride = sizeof(Vertex);
		mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		description.bindings.push_back(mainBinding);

		//Position will be stored at Location 0
		VkVertexInputAttributeDescription positionAttribute{};
		positionAttribute.binding = 0;
		positionAttribute.location = 0;
		positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
		positionAttribute.offset = offsetof(Vertex, position);

		//Normal will be stored at Location 1
		VkVertexInputAttributeDescription normalAttribute{};
		normalAttribute.binding = 0;
		normalAttribute.location = 1;
		normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
		normalAttribute.offset = offsetof(Vertex, normal);

		//Color will be stored at Location 2
		VkVertexInputAttributeDescription colorAttribute{};
		colorAttribute.binding = 0;
		colorAttribute.location = 2;
		colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
		colorAttribute.offset = offsetof(Vertex, color);

		description.attributes.push_back(positionAttribute);
		description.attributes.push_back(normalAttribute);
		description.attributes.push_back(colorAttribute);

		return description;
	}
};

struct Mesh {
	std::vector<Vertex>	_vertices;
	AllocatedBuffer _vertexBuffer;
	bool load_from_obj(const char* filename) {
		//attrib will contain the verte arrays of the file
		tinyobj::attrib_t attrib;
		//shapes contains the info for each separate object in the file
		std::vector<tinyobj::shape_t> shapes;
		//materials contains the information about the material of each shape, but we won't use it.
		std::vector<tinyobj::material_t>materials;

		//error and warning output from the load function
		std::string warn;
		std::string err;

		//load the OBJ file
		tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);
		//make sure to output warnings to the console, in case of issues in file
		if (!warn.empty()) {
			std::cout << "WARN: " << warn << std::endl;
		}

		//if we have an error, print it to console and break mesh loading.
		if (!err.empty()) {
			std::cerr << err << std::endl;
			return false;
		}

		//Loop over shapes
		for (size_t s = 0; s < shapes.size(); s++) {
			//Loop over faces (polygons)
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
				//harcode loading to triangles
				int fv = 3;
				//Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++) {
					//access vertex
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

					//vertex position
					tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
					tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
					tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
					//vertex normal
					tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
					tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
					tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

					//copy it to our vertex
					Vertex newVert;
					newVert.position.x = vx;
					newVert.position.y = vy;
					newVert.position.z = vz;
					newVert.normal.x = nx;
					newVert.normal.y = ny;
					newVert.normal.z = nz;
					newVert.color = newVert.normal;

					_vertices.push_back(newVert);
				}
				index_offset += fv;
			}
		}

		return true;
	}
};
