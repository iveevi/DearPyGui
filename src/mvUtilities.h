#pragma once

//-----------------------------------------------------------------------------
// mvUtilities
//
//     - This file contains typically platform specific functions. May need
//       to rename to a more appropriate name.
//     
//-----------------------------------------------------------------------------

#include <vector>
#include <string>
#include <imgui.h>      // Need this for ImTextureID
#include "mvTypes.h"

#ifndef PyObject_HEAD
struct _object;
typedef _object PyObject;
#endif

struct PymvBuffer;

// general
void FreeTexture(ImTextureID texture);
b8 UnloadTexture(const std::string& filename);
	
// static textures
ImTextureID LoadTextureFromFile(const char* filename, i32& width, i32& height);
ImTextureID LoadTextureFromArray(u32 width, u32 height, f32* data);

// dynamic textures
ImTextureID LoadTextureFromArrayDynamic(u32 width, u32 height, f32* data);
void  UpdateTexture(ImTextureID texture, u32 width, u32 height, std::vector<f32>& data);

// raw textures
ImTextureID LoadTextureFromArrayRaw(u32 width, u32 height, f32* data, i32 components);
void  UpdateRawTexture(ImTextureID texture, u32 width, u32 height, f32* data, i32 components);

// external GPU memory textures (Linux/OpenGL: GL_EXT_memory_object_fd)
// Imports an external memory fd (e.g. an opaque-fd exported from Vulkan) and
// creates a GL texture backed by that memory. The fd is consumed by the GL
// driver. Returns ImTextureID_Invalid on unsupported platforms or extension
// missing.
ImTextureID LoadTextureFromExternalMemoryFd(u32 width, u32 height, i32 fd, unsigned long long size_in_bytes, i32 components);

// framebuffer output
void OutputFrameBuffer(const char* filepath);
void OutputFrameBufferArray(PymvBuffer* out);
