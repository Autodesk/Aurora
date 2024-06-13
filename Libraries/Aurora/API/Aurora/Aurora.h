// Copyright 2024 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

// Aurora headers.
#include <Aurora/Foundation/Log.h>

// STL headers.
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// GLM - OpenGL Mathematics.
// NOTE: This is a math library, and not specific to OpenGL.
#define GLM_FORCE_CTOR_INIT
#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct/union
#include <glm/glm.hpp>
#pragma warning(pop)

// API import symbol.
#if !defined(AURORA_API)
#ifdef WIN32
#define AURORA_API __declspec(dllimport)
#else
#define AURORA_API
#endif
#endif

#include "AuroraNames.h"

// A macro to define smart pointer aliases, e.g. shared_ptr<IMaterial> gets a "IMaterialPtr" alias.
#define MAKE_AURORA_PTR(_x) using _x##Ptr = std::shared_ptr<_x>

namespace Aurora
{

/// Load resource function, used by the renderer to load a buffer for a resource (e.g. textures
/// loaded from MaterialX file)
///
/// \param uri Universal Resource Identifier (URI) for the resource to load.
/// \param pBufferOut The buffer containing the loaded resource.
/// \param pFileNameOut File name of the loaded resource containing the (used as hint for subsequent
/// processing)
/// \return True if loaded successfully.
using LoadResourceFunction = std::function<bool(
    const std::string& uri, std::vector<unsigned char>* pBufferOut, std::string* pFileNameOut)>;

// Math objects.
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using rgb  = glm::vec3;
using rgba = glm::vec4;

/// A unique identifier for Scene elements.
using Path     = std::string;
using PathView = std::string_view;

// Array of strings represented as vector.
using Strings = std::vector<std::string>;

/// Property value variant (of basic types).
struct PropertyValue
{
    enum class Type : uint8_t
    {
        Undefined,
        Bool,
        Int,
        Float,
        Float2,
        Float3,
        Float4,
        Matrix4,
        String,
        Strings
    };

    PropertyValue() : type(Type::Undefined) {}
    PropertyValue(const PropertyValue& in) { *this = in; }
    const PropertyValue& operator=(const PropertyValue& in)
    {
        type = in.type;
        switch (type)
        {
        default:
            break;
        case Type::Bool:
            _bool = in._bool;
            break;
        case Type::Int:
            _int = in._int;
            break;
        case Type::Float:
            _float = in._float;
            break;
        case Type::Float2:
            _float2 = in._float2;
            break;
        case Type::Float3:
            _float3 = in._float3;
            break;
        case Type::Float4:
            _float4 = in._float4;
            break;
        case Type::Matrix4:
            _matrix4 = in._matrix4;
            break;
        case Type::String:
            _string = in._string;
            break;
        case Type::Strings:
            _strings = in._strings;
            break;
        }
        return *this;
    }
    PropertyValue(bool value) : type(Type::Bool), _bool(value) {}
    PropertyValue(int value) : type(Type::Int), _int(value) {}
    PropertyValue(float value) : type(Type::Float), _float(value) {}
    PropertyValue(const vec2& value) : type(Type::Float2), _float2(value) {}
    PropertyValue(const vec3& value) : type(Type::Float3), _float3(value) {}
    PropertyValue(const vec4& value) : type(Type::Float4), _float4(value) {}
    PropertyValue(const mat4& value) : type(Type::Matrix4), _matrix4(value) {}
    PropertyValue(const char* value) : type(Type::String), _string(value) {}
    PropertyValue(const std::string& value) : type(Type::String), _string(value) {}
    PropertyValue(const Strings& value) : type(Type::Strings) { _strings = value; }

    // Allow construction of an undefined value by passing in nullptr (e.g. properties =
    // {{"clearMeProp", nullptr}})
    PropertyValue(nullptr_t) : type(Type::Undefined) {}

    /// The type of the value, or Type::Undefined if no value.
    Type type = Type::Undefined;

    /// Get the value as a string (will fail if not correct type.)
    const std::string& asString() const
    {
        AU_ASSERT(type == Type::String, "Not a string");
        return _string;
    }

    /// Get the value as a boolean (will fail if not correct type.)
    bool asBool() const
    {
        AU_ASSERT(type == Type::Bool, "Not a bool");
        return _bool;
    }

    /// Get the value as an int (will fail if not correct type.)
    int asInt() const
    {
        AU_ASSERT(type == Type::Int, "Not an int");
        return _int;
    }

    /// Get the value as a float (will fail if not correct type.)
    float asFloat() const
    {
        AU_ASSERT(type == Type::Float, "Not a float");
        return _float;
    }

    /// Get the value as a vec2 (will fail if not correct type.)
    vec2 asFloat2() const
    {
        AU_ASSERT(type == Type::Float2, "Not a float2");
        return _float2;
    }

    /// Get the value as a vec3 (will fail if not correct type.)
    vec3 asFloat3() const
    {
        AU_ASSERT(type == Type::Float3, "Not a float3");
        return _float3;
    }

    /// Get the value as a vec4 (will fail if not correct type.)
    vec4 asFloat4() const
    {
        AU_ASSERT(type == Type::Float4, "Not a float4");
        return _float4;
    }

    /// Get the value as a mat4 (will fail if not correct type.)
    mat4 asMatrix4() const
    {
        AU_ASSERT(type == Type::Matrix4, "Not a matrix4");
        return _matrix4;
    }

    /// Get the value as a string array (will fail if not correct type.)
    Strings& asStrings()
    {
        AU_ASSERT(type == Type::Strings, "Not a string array");
        return _strings;
    }

    /// Get the value as a const string array (will fail if not correct type.)
    const Strings& asStrings() const
    {
        AU_ASSERT(type == Type::Strings, "Not a string array");
        return _strings;
    }

    // Does this property have a value or is it undefined ?
    bool hasValue() { return type != Type::Undefined; }

    /// Clears the property value.
    void clear()
    {
        type = Type::Undefined;
        _string.clear();
        _strings.clear();
    }

    /// Compare property value for inequality, based on type.
    bool operator!=(const PropertyValue& in) const { return !(*this == in); }

    /// Compare property value for equality, based on type.
    bool operator==(const PropertyValue& in) const
    {
        // If types don't match never equal.
        if (type != in.type)
            return false;

        // Compare based on type value.
        switch (type)
        {
        case Type::Bool:
            return _bool == in._bool;
        case Type::Int:
            return _int == in._int;
        case Type::Float:
            return _float == in._float;
        case Type::Float2:
            return _float2 == in._float2;
        case Type::Float3:
            return _float3 == in._float3;
        case Type::Float4:
            return _float4 == in._float4;
        case Type::Matrix4:
            return _matrix4 == in._matrix4;
        case Type::String:
            return _string == in._string;
        case Type::Strings:
            // If string array lengths do not match, equality is false.
            if (_strings.size() != in._strings.size())
                return false;

            // If any string does not match equality is false.
            for (size_t i = 0; i < _strings.size(); i++)
            {
                if (_strings[i] != in._strings[i])
                    return false;
            }

            // Return true if all strings match.
            return true;
        default:
            // Invalid values are always non-equal.
            return false;
        }
    }

    /// Convert value to string.
    std::string toString() const
    {
        // Compare based on type value.
        switch (type)
        {
        case Type::Bool:
            return std::to_string(_bool);
        case Type::Int:
            return std::to_string(_int);
        case Type::Float:
            return std::to_string(_float);
        case Type::Float2:
            return std::to_string(_float2.x) + ", " + std::to_string(_float2.y);
        case Type::Float3:
            return std::to_string(_float3.x) + ", " + std::to_string(_float3.y) + ", " +
                std::to_string(_float3.z);
        case Type::Float4:
            return std::to_string(_float4.x) + ", " + std::to_string(_float4.y) + ", " +
                std::to_string(_float4.z) + ", " + std::to_string(_float4.w);
        case Type::Matrix4:
            return std::to_string(_matrix4[0][0]) + ", " + std::to_string(_matrix4[0][1]) + ", " +
                std::to_string(_matrix4[0][2]) + ", " + std::to_string(_matrix4[0][3]) + ", " +
                std::to_string(_matrix4[1][0]) + ", " + std::to_string(_matrix4[1][1]) + ", " +
                std::to_string(_matrix4[1][2]) + ", " + std::to_string(_matrix4[1][3]) + ", " +
                std::to_string(_matrix4[2][0]) + ", " + std::to_string(_matrix4[2][1]) + ", " +
                std::to_string(_matrix4[2][2]) + ", " + std::to_string(_matrix4[2][3]) + ", " +
                std::to_string(_matrix4[3][0]) + ", " + std::to_string(_matrix4[3][1]) + ", " +
                std::to_string(_matrix4[3][2]) + ", " + std::to_string(_matrix4[3][3]);

        case Type::String:
            return _string;
        case Type::Strings:
        {
            std::string res;
            // If any string does not match equality is false.
            for (size_t i = 0; i < _strings.size(); i++)
            {
                res += _strings[i];
                if (i < _strings.size())
                    res += ", ";
            }

            // Return true if all strings match.
            return res;
        }
        default:
            return "";
        }
    }

    union
    {
        bool _bool;
        int _int;
        float _float;
        vec2 _float2;
        vec3 _float3;
        vec4 _float4;
        mat4 _matrix4;
    };

    // These properties are outside the union due to being an element of variable size.
    // TODO: Should consider using std::variant.

    // String value (only valid if type is Type::String)
    std::string _string;

    // String array value (only valid if type is Type::Strings)
    Strings _strings;
};

// A collection of named properties, of varying types.
using Properties = std::map<std::string, PropertyValue>;

/// Input vertex attribute formats.
enum class AttributeFormat : uint8_t
{
    // Signed 8-bit integer.
    SInt8,

    // Unsigned 8-bit integer.
    UInt8,

    // Signed 16-bit integer.
    SInt16,

    // Unsigned 16-bit integer.
    UInt16,

    // Signed 32-bit integer.
    SInt32,

    // Unsigned 32-bit integer.
    UInt32,

    // Single precision float.
    Float,

    // Single precision 2D point.
    Float2,

    // Single precision 3D point.
    Float3,

    // Single precision 4D point.
    Float4
};

/// A collection of primitive types.
enum class PrimitiveType : uint8_t
{
    // An array of points.
    Points,

    // An array of separate lines.
    Lines,

    // An array of connected lines.
    Linestrip,

    // An array of separate triangles.
    Triangles,

    // An array of connected triangles.
    Trianglestrip
};

/// Input image pixel formats.
enum class ImageFormat : uint8_t
{
    /// 8-bit per-channel, normalized byte.
    Byte_R,

    /// 8-bit per-channel, 4-channel normalized integer.
    Integer_RGBA,

    /// 32-bit per-channel, 2-channel normalized integer.
    Integer_RG,

    /// 16-bit per-channel, 4-channel normalized short.
    Short_RGBA,

    /// 16-bit per-channel, 4-channel half-float.
    Half_RGBA,

    /// 32-bit per-channel, 4-channel float.
    Float_RGBA,

    /// 32-bit per-channel, 3-channel float.
    Float_RGB,

    /// 32-bit per-channel, single channel float.
    Float_R
};

/// Input vertex description. Defines which vertex attributes each vertex has, and the number of
/// vertices.
struct VertexDescription
{
    /// Map of named vertex attributes.
    std::unordered_map<std::string, AttributeFormat> attributes;

    /// The total number of vertices.
    size_t count = 0;

    bool hasAttribute(const std::string& name) const
    {
        return attributes.find(name) != attributes.end();
    }
};

/// \struct Vertex attribute data for single attribute, filled in by client in
/// GetAttributeDataFunction.
struct AttributeData
{
    /// Pointer to actual vertex or index attribute data.
    const void* address = nullptr;

    /// Offset in bytes to start of attribute data.
    size_t offset = 0;

    /// The total size in bytes of the provided buffer, used for validation.
    size_t size = 0;

    /// The stride in bytes between attributes.  If left as zero assumed to be exactly the size of
    /// attribute.
    size_t stride = 0;
};

/// Map of attribute data for all the vertex  and index attributes, filed in by getVertexData
/// callback when geometry is activated.
using AttributeDataMap = std::map<std::string, AttributeData>;

/// Callback function to get the vertex and index attribute data for a geometry object from the
/// client.
///
/// \param dataOut A map of AttributeData containing an uninitialized entry for each vertex and
/// index attribute.  Should be filled in by client providing the vertex and index data required.
/// \param firstVertex The first vertex data is required for.  Currently always zero.
/// \param vertexCount The number of vertices data is required for.  Currently always vertex count
/// provided in descriptor.
/// \param firstIndex The first index data is required for.  Currently
/// always zero.
/// \param lastIndex The last index data is required for.  Currently always vertex
/// count provided in descriptor.
/// \return false if client was not able to create attribute data.
using GetAttributeDataFunction = std::function<bool(AttributeDataMap& dataOut, size_t firstVertex,
    size_t vertexCount, size_t firstIndex, size_t indexCount)>;

/// Callback function to signal the vertex and index attribute data for the geometry object has been
/// updated, and the pointers provided by the GetAttributeDataFunction can be freed by the client.
/// Provides the same arguments that were provided by getAttributeData.
using AttributeUpdateCompleteFunction = std::function<void(const AttributeDataMap& dataOut,
    size_t firstVertex, size_t vertexCount, size_t firstIndex, size_t indexCount)>;

/// Input geometry description.
struct GeometryDescriptor
{
    /// The primitive and topology.
    PrimitiveType type = PrimitiveType::Triangles;

    /// Complete description of vertex attributes.
    VertexDescription vertexDesc;

    /// Number of indices.
    size_t indexCount = 0;

    /// Callback for getting the vertex attribute data from the client, called when geometry
    /// resource is activated.
    GetAttributeDataFunction getAttributeData = nullptr;

    /// Optional completion callback, called after geometry resource is activated and vertex
    /// attribute data is no longer in use by the renderer and can be freed by the client.
    AttributeUpdateCompleteFunction attributeUpdateComplete = nullptr;
};

/// \struct Image data description, filed in by getData callback when IImage is activated.
struct ImageData
{
    /// Pixel buffer address.
    const void* pPixelBuffer = nullptr;

    /// Size of one row of pixels.
    size_t bytesPerRow = 0;

    /// The total size in bytes of the pixel buffer.
    size_t bufferSize = 0;

    /// Size of the image.
    glm::ivec2 dimensions = glm::ivec2(0, 0);

    /// The format of the image.
    ImageFormat format = ImageFormat::Integer_RGBA;

    /// Should we override the linearize flag the image was created with.
    bool overrideLinearize = false;

    /// Whether the image should be linearized from sRGB to linear color space.
    /// Ignored unless overrideLinearize is set.
    bool linearize = true;
};

/// Callback function, passed to the getData callback functions to allocate buffers to use in the
/// returned data (e.g. pixel buffers).
///
/// \param size  The size of buffer to be allocated.
using AllocateBufferFunction = std::function<void*(size_t size)>;

/// Callback function to get the data, including size and pixel buffer, for an image object from the
/// client.
///
/// \param dataOut  The image data, including pointer to actual pixels, to be filled in by client.
using GetImageDataFunction = std::function<bool(ImageData& dataOut, AllocateBufferFunction alloc)>;

/// Callback function to signal the vertex and index attribute data for the geometry object has been
/// updated, and the pointers provided by the GetAttributeDataFunction can be freed by the client.
/// Provides the same arguments that were provided by getAttributeData.
using ImageUpdateCompleteFunction = std::function<void()>;

/// Input image description.
struct ImageDescriptor
{
    /// Whether the image should be linearized from sRGB to linear color space.
    bool linearize = true;

    /// Whether the image is to be used to represent an environment.
    bool isEnvironment = false;

    /// Callback for getting the pixel data, called when geometry resource is activated.
    GetImageDataFunction getData = nullptr;

    /// Optional completion callback, called after image resource is activated and image pixel data
    /// is no longer in use by the renderer.
    ImageUpdateCompleteFunction updateComplete = nullptr;
};

/// Instance definition, all the data required to create an instance resource.
struct InstanceDefinition
{
    Path path;
    Properties properties;
};

// Vector of instance definitions.
using InstanceDefinitions = std::vector<InstanceDefinition>;

// Vector of paths.
using Paths = std::vector<Path>;

// A class representing an image for use in the rendering pipeline.
class AURORA_API IImage
{
public:
    /// Structure containing the information required to initialize an image.
    /// TODO: This should use ImageDescriptor directly.
    struct InitData
    {
        /// The image pixels.
        /// \note This data is not retained by the renderer after the image is created.
        const void* pImageData = nullptr;

        /// The format of the image.
        ImageFormat format = ImageFormat::Integer_RGBA;

        /// Whether the image should be linearized from sRGB to linear color space.
        bool linearize;

        /// Whether the image is to be used to represent an environment.
        bool isEnvironment = false;

        /// The width of image in pixels.
        uint32_t width = 0;

        /// The height of image in pixels.
        uint32_t height = 0;

        /// The name of the image.
        /// \note This is for client reference only, and does not need to be unique.
        std::string name;
    };

protected:
    virtual ~IImage() = default; // hidden destructor
};
MAKE_AURORA_PTR(IImage);

// A class representing an sampler for use in the rendering pipeline, defining texture sampling
// parameters.
class AURORA_API ISampler
{
public:
    virtual ~ISampler() = default; // hidden destructor
};
MAKE_AURORA_PTR(ISampler);

// A class representing a set of values for fixed properties of various types. The derived class
// defines the properties and default values.
class AURORA_API IValues
{
public:
    // An enumeration of types for properties and values.
    enum class Type
    {
        Undefined,
        Boolean,
        Int,
        Float,
        Float2,
        Float3,
        Matrix,
        Image,
        Sampler,
        String
    };

    virtual void setBoolean(const std::string& name, bool value)               = 0;
    virtual void setInt(const std::string& name, int value)                    = 0;
    virtual void setFloat(const std::string& name, float value)                = 0;
    virtual void setFloat2(const std::string& name, const float* value)        = 0;
    virtual void setFloat3(const std::string& name, const float* value)        = 0;
    virtual void setMatrix(const std::string& name, const float* value)        = 0;
    virtual void setImage(const std::string& name, const IImagePtr& value)     = 0;
    virtual void setSampler(const std::string& name, const ISamplerPtr& value) = 0;
    virtual void setString(const std::string& name, const std::string& value)  = 0;
    virtual void clearValue(const std::string& name)                           = 0;

    virtual Type type(const std::string& name) = 0;

protected:
    virtual ~IValues() = default; // hidden destructor
};
MAKE_AURORA_PTR(IValues);

// A buffer that can receive a rendered image.
class AURORA_API ITarget
{
public:
    virtual void resize(uint32_t width, uint32_t height) = 0;

protected:
    virtual ~ITarget() = default; // hidden destructor
};
MAKE_AURORA_PTR(ITarget);

// A window handle, used when creating window targets.
using WindowHandle = void*;

// A class representing a target that is associated with an operating system window.
class AURORA_API IWindow : public ITarget
{
public:
    virtual void setVSyncEnabled(bool enabled) = 0;

protected:
    virtual ~IWindow() = default; // hidden destructor
};
MAKE_AURORA_PTR(IWindow);

// A class representing a target that can have its data accessed by the client. For example, this is
// useful for saving screenshots to disk.
class AURORA_API IRenderBuffer : public ITarget
{
public:
    /// Class used to hold a readable buffer.
    /// If the this class is holding a readback buffer it handles mapping and unmapping data for
    /// readback based on the lifetime of the object.
    /// It may also hold a handle to a GPU buffer
    class IBuffer
    {
    public:
        /// The raw data pointer.
        virtual const void* data() = 0;

        // Handle to a GPU buffer
        virtual const void* handle() { return nullptr; }

    protected:
        IBuffer()          = default;
        virtual ~IBuffer() = default;
    };
    using IBufferPtr = std::shared_ptr<IRenderBuffer::IBuffer>;

    /// Get the contents of the render buffer on the CPU.
    /// If removePadding is true the returned data will match image dimensions without any extra
    /// padding at the end of each row.  stride will return the stride in bytes of each row (if
    /// removePadding is true this will always equal row length of image).
    virtual const void* data(size_t& stride, bool removePadding = false) = 0;

    /// Returns a class holding GPU buffer data that is ready to read.  The IBuffer class
    /// handles mapping and unmapping data for readback based on the lifetime of the object.
    virtual IBufferPtr asReadable(size_t& stride) = 0;

    /// Returns a class holding a handle to shareable GPU buffer memory.
    virtual IBufferPtr asShared() = 0;

protected:
    virtual ~IRenderBuffer() = default; // hidden destructor
};
MAKE_AURORA_PTR(IRenderBuffer);

// A class representing triangle geometry, as vertex data buffers and an index buffer.
class AURORA_API IGeometry
{
public:
protected:
    virtual ~IGeometry() = default; // hidden destructor
};
MAKE_AURORA_PTR(IGeometry);

// A class representing the environment that surrounds the scene, including lighting and background.
// Properties include the following:
// - light_top (float3): The top color of the gradient used for lighting.
// - light_bottom (float3): The bottom color of the gradient used for lighting.
// - light_image (IImage): The image used for lighting, which takes precedence over the gradient.
// - light_transform (float4x4):  A transformation to apply to the lighting.
// - background_top (float3): The top color of the gradient used for the background.
// - background_bottom (float3): The top color of the gradient used for the background.
// - background_image (IImage): The image used for the background, which takes precedence over the
//   gradient.
// - background_transform (float4x4): A transformation to apply to the background.
// - background_use_screen (bool): Whether to use a simple screen mapping for the background;
//   otherwise a wraparound spherical mapping is used (default false).
class AURORA_API IEnvironment
{
public:
    virtual IValues& values() = 0;

protected:
    virtual ~IEnvironment() = default; // hidden destructor
};
MAKE_AURORA_PTR(IEnvironment);

/// A class representing a infinite ground plane in the scene, that acts as a "catcher" for shadows
/// and reflections of the scene, but is otherwise invisible.
///
/// Properties include the following:
/// - position: An arbitrary point on the plane.
/// - normal: The normal of the plane, which points away from the rendered side of the plane. The
///   other side is not rendered, i.e. is completely invisible.
/// - shadow_opacity: The visibility of the matte shadow effect in [0.0, 1.0].
/// - shadow_color: The tint color of the matte shadow effect. This is normally black.
/// - reflection_opacity: The visibility of the matte reflection effect in [0.0, 1.0].
/// - reflection_color: The tint color of the matte reflection effect. This is normally white.
/// - reflection_roughness: The GGX roughness of the matte reflection effect in [0.0, 1.0].
class AURORA_API IGroundPlane
{
public:
    virtual IValues& values() = 0;

protected:
    virtual ~IGroundPlane() = default; // hidden destructor
};
MAKE_AURORA_PTR(IGroundPlane);

/// A class representing a material that can be assigned to an instance, that determines surface
/// appearance (shading) of the instance.
class AURORA_API IMaterial
{
public:
    /// Gets the values of the material's properties, which can be read and modified as needed.
    ///
    /// For some materials the properties are those defined by Autodesk Standard Surface, e.g.
    /// "base_color" and "specular_roughness."
    virtual IValues& values() = 0;

protected:
    virtual ~IMaterial() = default; // hidden destructor
};
MAKE_AURORA_PTR(IMaterial);

// Definition of an instance layer (material+geometry)
using LayerDefinition = std::pair<IMaterialPtr, IGeometryPtr>;

// Array of layer definitions.
using LayerDefinitions = std::vector<LayerDefinition>;

/// A class representing an instance of geometry, including a per-instance material and transform.
class AURORA_API IInstance
{
public:
    /// Sets the material of the instance.
    ///
    /// The material to assign. Setting to null (the default) refers to a simple default material.
    virtual void setMaterial(const IMaterialPtr& pMaterial) = 0;

    /// Sets the transform of the instance, as a 4x4 transformation matrix.
    ///
    /// \param pTransform The transformation matrix to assign. The array must be in column-major
    /// layout, with column vectors. Setting to null (the default) uses the identity matrix.
    virtual void setTransform(const mat4& pTransform) = 0;

    /// Sets the unique integer identifier for the instance, which is used for selection
    ///
    /// \param objectId The object id to assign.
    virtual void setObjectIdentifier(int objectId) = 0;

    /// Makes the instance visible or invisible.
    ///
    /// \param visible The object id to assign.
    virtual void setVisible(bool visible) = 0;

    virtual IGeometryPtr geometry() const = 0;

protected:
    virtual ~IInstance() = default; // hidden destructor
};
MAKE_AURORA_PTR(IInstance);

/// A class representing a light.  Which are added to the scene to provide direct illumination.
class AURORA_API ILight
{
public:
    /// Gets the lights's properties, which can be read and modified as needed.
    ///
    /// The properties are specific to the light type.
    virtual IValues& values() = 0;

protected:
    virtual ~ILight() = default; // hidden destructor
};
MAKE_AURORA_PTR(ILight);

/// The types of resources that can be associated with an Aurora scene (directly or indirectly).
enum ResourceType
{
    Material = 0,
    Instance,
    Environment,
    Geometry,
    Sampler,
    GroundPlane,
    Image,
    Light,
    Invalid
};

/// A class representing a scene for rendering, consisting of instances of geometry, an environment,
/// and a global directional light.
class AURORA_API IScene
{
public:
    /// Get the resource type for the given path.
    /// \param atPath The path to be queried.
    /// \return The type of resource, or ResourceType::Invalid if no resource for given path.
    virtual ResourceType getResourceType(const Path& atPath) = 0;

    /// Set the image descriptor for image with given path.
    /// This will create a new image (or force it to be recreated an image already exists at that
    /// path).
    ///
    /// \param atPath The path at which the image will be created.
    /// \param desc A description of the image.
    virtual void setImageDescriptor(const Path& atPath, const ImageDescriptor& desc) = 0;

    /// Create an image with the given file path.  The image descriptor will be created using the
    /// default load callback provided by setLoadResourceFunction.
    ///
    /// \param atPath The Aurora path at which the image will be created.
    /// \param filePath The path to the image file, if the string is empty, then atPath is used.
    virtual void setImageFromFilePath(const Path& atPath, const std::string& filePath = "",
        bool linearize = true, bool isEnvironment = false) = 0;

    /// Set the properties for sampler with given path.
    /// This will create a new sampler (or force it to be recreated an sampler already exists at
    /// that path).
    ///
    /// \param atPath The path at which the sampler will be created.
    /// \param props Sampler properties
    virtual void setSamplerProperties(const Path& atPath, const Properties& props) = 0;

    /// Set the material type and document for material with given path.
    /// This will create a new material (or recreate it if one already exists at that path).
    /// \param atPath The path at which the material will be added.
    /// \param materialType Type of material (one of strings for Material properties).
    /// \param document Document describing the material (based on value of materialType).
    virtual void setMaterialType(const Path& atPath,
        const std::string& materialType = Names::MaterialTypes::kBuiltIn,
        const std::string& document     = "Default") = 0;

    /// Set geometry descriptor, if geometry already exists it will be recreated.
    /// \param atPath The path at which the geometry will be created.
    /// \param desc A description of the geometry.
    virtual void setGeometryDescriptor(const Path& atPath, const GeometryDescriptor& desc) = 0;

    /// Prevents the resource from being purged by the renderer if unused (can be nested).
    virtual void addPermanent(const Path& resource) = 0;

    /// Restores the resource's purgeability.
    virtual void removePermanent(const Path& resource) = 0;

    /// Adds an instance of specified geometry.
    /// \param atPath The path at which to create the geometry.
    /// \param geometry A path to the geometry description.
    /// \param properties Instance property settings.
    /// \returns True if successful.
    virtual bool addInstance(
        const Path& atPath, const Path& geometry, const Properties& properties = {}) = 0;

    /// Adds several instances with specified geometry.
    /// \param atPath The path at which to create the geometry.
    /// \param geometry A path to the geometry description.
    /// \param properties Instance property settings.
    /// \returns True if successful.
    virtual Paths addInstances(const Path& geometry, const InstanceDefinitions& definitions) = 0;

    /// Sets the specified properties for the environment at the path.  Environment will be created
    /// if none exists at that path.
    ///
    /// \param environment The environment path.
    /// \returns True if successful.
    virtual bool setEnvironmentProperties(
        const Path& environment, const Properties& environmentProperties) = 0;

    /// Sets the environment (singleton) to be used for the scene.
    ///
    /// This will cause the resources associated with the environment, including any images, to be
    /// created.
    ///
    /// \param environment The environment path. Uses default environment if path is empty.
    /// \returns True if successful.
    virtual bool setEnvironment(const Path& environment) = 0;

    /// Removes the instance at the specified path from the scene, so that it is no longer
    /// rendered.
    /// The associated resources (including referenced geometry and materials) will be destroy once
    /// they are no longer attached to scene.
    /// \param path The path to remove from the scene. Nothing
    /// is done if the path is
    /// not in the scene.
    virtual void removeInstance(const Path& path) = 0;

    /// Removes the instance at the specified set of paths from the scene, so that they are no
    /// longer rendered.
    /// The associated resources (including referenced geometry and materials) will be destroy once
    /// they are no longer attached to scene.
    ///
    /// \param path The paths to remove from the scene. Nothing is done if a path is
    /// not in the scene.
    virtual void removeInstances(const Paths& paths) = 0;

    /// Sets the specified properties of the material.
    /// This will create a new material if one does not exist for that path.
    /// \param The path to the material.
    /// \param materialProperties The set of properties to change.
    virtual void setMaterialProperties(const Path& path, const Properties& materialProperties) = 0;

    /// Sets the specified properties of the instance.
    /// \param path The path to instance to modify (will fail if path does not exist).
    /// \param instanceProperties The set of properties to modify.
    virtual void setInstanceProperties(const Path& path, const Properties& instanceProperties) = 0;

    /// Sets the specified properties of the instances.
    /// \param paths The paths to the instances to modify (will fail if path does not exist).
    /// \param instanceProperties The set of properties to modify.
    virtual void setInstanceProperties(
        const Paths& paths, const Properties& instanceProperties) = 0;

    /// Sets the bounding box for the scene.
    ///
    /// \note The client is expected to update the bounding box as it changes. Not doing so may
    /// result in undefined behavior. This may be updated internally in the future.
    virtual void setBounds(const vec3& min, const vec3& max) = 0;

    /// Sets the bounding box for the scene.
    ///
    /// TODO: Remove. Replaced by GLM types.
    /// \note The client is expected to update the bounding box as it changes. Not doing so may
    /// result in undefined behavior. This may be updated internally in the future.
    virtual void setBounds(const float* min, const float* max) = 0;

    /// Sets the ground plane for the scene.
    virtual void setGroundPlanePointer(const IGroundPlanePtr& pGroundPlane) = 0;

    virtual IInstancePtr addInstancePointer(const Path& path, const IGeometryPtr& geom,
        const IMaterialPtr& pMaterial, const mat4& pTransform,
        const LayerDefinitions& materialLayers = {}) = 0;

    /// Add a new light to add illumination to the scene.
    /// The light will be removed from the scene, when the shared pointer becomes unused.
    ///
    /// \param lightType Type of light (one of strings in
    /// Aurora::Names::LightTypes).
    /// \return A smart pointer to the new lights.
    virtual ILightPtr addLightPointer(const std::string& lightType) = 0;

protected:
    virtual ~IScene() = default; // hidden destructor
};
MAKE_AURORA_PTR(IScene);

/// Arbitrary output variables (AOV): Types of data that can be produced from the renderer and
/// written to targets.
///
/// \note The name "AOV" refers to a source of data. A target is assigned to and is filled with data
/// from an AOV, but the target itself is not technically an AOV.
enum class AOV
{
    /// The final, user-visible result of rendering, sometimes called "beauty".
    kFinal,

    /// The depth of scene geometry in normalized device coordinates (NDC), as determined by the
    /// provided projection matrix.
    kDepthNDC
};

/// A map of AOVs to targets, to indicate which targets should receive which AOVs.
using TargetAssignments = std::unordered_map<AOV, ITargetPtr>;

// A class for rendering images and creating objects associated with the renderer.
class AURORA_API IRenderer
{
public:
    /// Type of rendering backend used by Aurora.
    enum class Backend
    {
        // USD HGI rendering backend (uses for Vulkan).
        HGI,
        // DirectX DXR rendering backend.
        DirectX,
        // Choose default backend for current platform.
        Default
    };

    /// Create rendering window from provided OS windows handle.
    virtual IWindowPtr createWindow(WindowHandle handle, uint32_t width, uint32_t height) = 0;

    /// Create a new render buffer object, that can be used as a target for the renderer.
    /// \param width Width of the render buffer in pixels.
    /// \param height Height of the render buffer in pixels.
    /// \desc The buffer image format.
    /// \return A smart pointer to the new render buffer
    virtual IRenderBufferPtr createRenderBuffer(int width, int height, ImageFormat format) = 0;

    /// Create a new image object, that can be used as a material texture or as part of the
    /// environment.
    ///
    /// \note This uses the resource pointer directly, the preferred usage is to access via the path
    /// API in IScene instead.
    /// \param initData The data required to create image.
    /// \return A smart pointer to the new image.
    /// TODO: The pointer functions should be hidden and only accessibly from the implementation,
    /// not the public interface.
    virtual IImagePtr createImagePointer(const IImage::InitData& initData) = 0;

    /// Create a new sampler object.
    ///
    /// \note This uses the resource pointer directly, the preferred usage is to access via the path
    /// API in IScene instead.
    /// \param props The sampler's properties.
    /// \return A smart pointer to the new sampler.
    /// TODO: The pointer functions should be hidden and only accessibly from the implementation,
    /// not the public interface.
    virtual ISamplerPtr createSamplerPointer(const Properties& props) = 0;

    /// Create a new material, which describes the appearance of an instance.
    ///
    /// \note This uses the resource pointer directly, the preferred usage is to access via the path
    /// API in IScene instead.
    /// \param materialType Type of material (one of strings in
    /// Aurora::Names::MaterialTypes).
    /// \param document Document describing the material (interpreted based on value of
    /// materialType.)
    /// \param name Name of material.
    /// \return A smart pointer to the new material.
    /// TODO: The pointer functions should be hidden and only accessibly from the implementation,
    /// not the public interface.
    virtual IMaterialPtr createMaterialPointer(
        const std::string& materialType = Names::MaterialTypes::kBuiltIn,
        const std::string& document = "Default", const std::string& name = "") = 0;

    /// Creates a new environment object that describes the infinite area lighting and background of
    /// a scene.
    ///
    /// \note This uses the resource pointer directly, the preferred usage is to access via the path
    /// API in IScene instead.
    /// \return A smart pointer to the new render buffer
    /// TODO: The pointer functions should be hidden and only accessibly from the implementation,
    /// not the public interface.
    virtual IEnvironmentPtr createEnvironmentPointer() = 0;

    /// Creates a new ground plane object that describes a virtual plane that shows shadows and
    /// reflections of the scene.
    virtual IGeometryPtr createGeometryPointer(
        const GeometryDescriptor& desc, const std::string& name = "") = 0;

    /// Creates a new ground plane object that describes a virtual plane that shows shadows and
    /// reflections of the scene.
    virtual IGroundPlanePtr createGroundPlanePointer() = 0;

    /// Creates a new scene for rendering.
    virtual IScenePtr createScene() = 0;

    /// Sets renderer options.
    virtual void setOptions(const Properties& options) = 0;

    /// TODO: Remove. This has been replaced by setOptions().
    virtual IValues& options() = 0;

    /// Gets the backend used by the renderer.
    virtual Backend backend() const = 0;

    // Set the scene to be rendered.
    virtual void setScene(const IScenePtr& pScene) = 0;

    /// Sets the AOVs that will be written to the associated targets when rendering is performed.
    ///
    /// Each AOV can be written to at most one target.
    virtual void setTargets(const TargetAssignments& targetAssignments) = 0;

    /// Sets the camera properties.
    /// \param view View matrix
    /// \param projection Projection matrix
    /// \param focalLength The distance between the camera pinhole and the image plane.
    /// \param lensRadius Lens radius of curvature (for depth of field).
    virtual void setCamera(const mat4& view, const mat4& projection, float focalDistance = 1.0f,
        float lensRadius = 0.0f) = 0;

    // TODO: Remove. This has been replaced by GM types.
    virtual void setCamera(const float* view, const float* proj, float focalDistance = 1.0f,
        float lensRadius = 0.0f) = 0;
    
    virtual void setFrameIndex(int frameIndex) = 0;

    // Render the current scene.
    virtual void render(uint32_t sampleStart = 0, uint32_t sampleCount = 1) = 0;

    // Wait for all the currently executing render tasks on the GPU to complete.
    virtual void waitForTask() = 0;

    /// \desc Get the supported set of built-in materials for this renderer, can be passed as a
    /// document to setMaterialType.
    /// \return Vector of built-in material names.
    virtual const std::vector<std::string>& builtInMaterials() = 0;

    /// \desc Set the callback function used to load resources, such as textures, from a URI.
    /// \param func Callback function to be used for all subsequent loading.
    virtual void setLoadResourceFunction(LoadResourceFunction func) = 0;

protected:
    virtual ~IRenderer() = default; // hidden destructor
};
MAKE_AURORA_PTR(IRenderer);

// Gets the logger for the Aurora library, used to report console output and errors.
AURORA_API Foundation::Log& logger();

// Creates a renderer with the specified backend and number of simultaneously active tasks.
AURORA_API IRendererPtr createRenderer(
    IRenderer::Backend type = IRenderer::Backend::Default, uint32_t taskCount = 3);

} // namespace Aurora
