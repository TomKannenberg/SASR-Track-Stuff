#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SeEditor::Forest {

enum class D3DDeclType : std::uint8_t
{
    Float1,
    Float2,
    Float3,
    Float4,
    D3DColor,
    UByte4,
    Short2,
    Short4,
    UByte4N,
    Short2N,
    Short4N,
    UShort2N,
    UShort4N,
    UDec3,
    Dec3N,
    Float16x2,
    Float16x4
};

enum class D3DDeclMethod : std::uint8_t
{
    Default,
    PartialU,
    PartialV,
    CrossUV,
    UV,
    Lookup,
    LookupPresampled
};

enum class D3DDeclUsage : std::uint8_t
{
    Position,
    BlendWeight,
    BlendIndices,
    Normal,
    Psize,
    TexCoord,
    Tangent,
    BiNormal,
    TessFactor,
    PositionT,
    Color,
    Fog,
    Depth,
    Sample
};

enum class XboxDeclType : std::int32_t
{
    Float1 = 0x2C83A4,
    Float2 = 0x2C23A5,
    Float3 = 0x2A23B9,
    Float4 = 0x1A23A6,
    Int1 = 0x2C83A1,
    Int2 = 0x2C23A2,
    Int4 = 0x1A23A3,
    UInt1 = 0x2C82A1,
    UInt2 = 0x2C22A2,
    UInt4 = 0x1A22A3,
    Int1N = 0x2C81A1,
    Int2N = 0x2C21A2,
    Int4N = 0x1A21A3,
    UInt1N = 0x2C80A1,
    UInt2N = 0x2C20A2,
    UInt4N = 0x1A20A3,
    D3DColor = 0x182886,
    UByte4 = 0x1A2286,
    Byte4 = 0x1A2386,
    UByte4N = 0x1A2086,
    Byte4N = 0x1A2186,
    Short2 = 0x2C2359,
    Short4 = 0x1A235A,
    UShort2 = 0x2C2259,
    UShort4 = 0x1A225A,
    Short2N = 0x2C2159,
    Short4N = 0x1A215A,
    UShort2N = 0x2C2059,
    UShort4N = 0x1A205A,
    UDec3 = 0x2A2287,
    Dec3 = 0x2A2387,
    UDec3N = 0x2A2087,
    Dec3N = 0x2A2187,
    UDec4 = 0x1A2287,
    Dec4 = 0x1A2387,
    UDec4N = 0x1A2087,
    Dec4N = 0x1A2187,
    UHenD3 = 0x2A2290,
    HenD3 = 0x2A2390,
    UHenD3N = 0x2A2090,
    HenD3N = 0x2A2190,
    UDHen3 = 0x2A2291,
    DHen3 = 0x2A2391,
    UDHen3N = 0x2A2091,
    DHen3N = 0x2A2191,
    Float16x2 = 0x2C235F,
    Float16x4 = 0x1A2360,
    Unused = -1
};

struct D3DVertexElement
{
    std::int16_t Stream = 0;
    std::int16_t Offset = 0;
    D3DDeclType Type = D3DDeclType::Float3;
    D3DDeclMethod Method = D3DDeclMethod::Default;
    D3DDeclUsage Usage = D3DDeclUsage::Position;
    std::uint8_t UsageIndex = 0;

    static int GetTypeSize(D3DDeclType type);
};

struct XboxVertexElement
{
    std::int16_t Stream = 0;
    std::int16_t Offset = 0;
    XboxDeclType Type = XboxDeclType::Float3;
    D3DDeclMethod Method = D3DDeclMethod::Default;
    D3DDeclUsage Usage = D3DDeclUsage::Position;
    std::uint8_t UsageIndex = 0;

    static int GetTypeSize(XboxDeclType type);
    static D3DDeclType MapTypeToD3D(XboxDeclType type);
};

struct SuNameFloatPairs;
struct SuNameUint32Pairs;
struct MaximalTreeCollisionOOBB;
struct SuBlindData;
struct SuRenderTextureResource;
struct SuRenderTexture;
struct SuRenderMaterial;
struct SuRenderVertexStream;
struct SuRenderPrimitive;
struct SuRenderMesh;
struct SuLodThreshold;
struct SuLodBranch;
struct SuBranch;
struct SuRenderTree;
struct SuTreeGroup;
struct SuTextureTransform;
struct SuAnimation;
struct SuAnimationEntry;
struct StreamOverride;
struct SuCollisionMesh;
struct SuCollisionTriangle;
struct SuLightData;
struct SuCameraData;
struct SuEmitterData;
struct SuCurve;
struct SuField;
struct SuRamp;

struct SuNameFloatPairs
{
    std::vector<std::pair<int, float>> Pairs;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuNameUint32Pairs
{
    std::vector<std::pair<int, int>> Pairs;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct MaximalTreeCollisionOOBB
{
    SlLib::Math::Matrix4x4 ObbTransform{};
    SlLib::Math::Vector4 Extents{};
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuBlindData
{
    static constexpr int MaximalTreeCollisionOobbTypeHash = -0x6F8177CE;
    static constexpr int FloatPairsInstanceHash = -0x265A4D45;
    static constexpr int UintPairsInstanceHash = 0x7796ebea;

    struct Element
    {
        int InstanceHash = 0;
        int TypeHash = 0;
        std::shared_ptr<void> Data;
    };

    std::vector<Element> Elements;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRenderTextureResource
{
    std::string Name;
    std::vector<std::uint8_t> ImageData;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRenderTexture
{
    int Flags = 0;
    std::shared_ptr<SuRenderTextureResource> TextureResource;
    int Param0 = 0;
    int Param1 = 0;
    int Param2 = 0;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRenderMaterial
{
    std::string Name;
    std::vector<std::shared_ptr<SuRenderTexture>> Textures;
    int PixelShaderFlags = 0;
    int Hash = 0;
    std::vector<float> FloatList;
    std::array<std::vector<int>, 6> Layers;

    int Unknown_0x44 = 0;
    int Unknown_0x48 = 0;
    int Unknown_0x4c = 0;
    std::uint8_t Unknown_0x51 = 0;
    int Unknown_0x54 = 0;
    int Unknown_0x64 = 0;
    std::uint8_t Unknown_0x68 = 0;
    std::uint8_t Unknown_0x69 = 0;
    std::uint8_t Unknown_0x6a = 0;
    std::uint8_t Unknown_0x6b = 0;

    std::vector<int> UnknownNumberList;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRenderVertexStream
{
    struct VertexStreamHashes
    {
        int Flags = 0;
        int NumStreams = 0;
        void Load(SlLib::Serialization::ResourceLoadContext& context);
        int GetSizeForSerialization() const;
    };

    std::vector<D3DVertexElement> AttributeStreamsInfo;
    std::vector<std::uint8_t> Stream;
    std::vector<std::uint8_t> ExtraStream;
    int VertexStride = 0;
    int VertexCount = 0;
    int StreamBias = 0;
    int NumExtraStreams = 0;
    int VertexStreamFlags = 0;
    std::shared_ptr<VertexStreamHashes> StreamHashes;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRenderPrimitive
{
    SlLib::Math::Matrix4x4 ObbTransform{};
    SlLib::Math::Vector4 Extents{};
    SlLib::Math::Vector4 CullSphere{};
    std::array<std::uint8_t, 8> TextureMatrixIndices{};
    struct TextureUVMap { std::uint8_t In=0xFF, Out=0xFF, Mat=0xFF; };
    std::array<TextureUVMap, 8> TextureUvMaps{};
    int TextureUvMapHashVS = 0;
    int TextureUvMapHashPS = 0;
    std::shared_ptr<SuRenderMaterial> Material;
    int NumIndices = 0;
    std::vector<std::uint8_t> IndexData;
    std::shared_ptr<SuRenderVertexStream> VertexStream;
    int Unknown_0x9c = 0;
    int Unknown_0xa0 = 0;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRenderMesh
{
    SlLib::Math::Vector4 BoundingSphere{};
    std::vector<std::shared_ptr<SuRenderPrimitive>> Primitives;
    std::vector<int> BoneMatrixIndices;
    std::vector<SlLib::Math::Matrix4x4> BoneInverseMatrices;
    std::string Name;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuLodThreshold
{
    float ThresholdDistance = 0.0f;
    std::shared_ptr<SuRenderMesh> Mesh;
    std::int16_t ChildBranch = -1;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuLodBranch
{
    std::vector<std::shared_ptr<SuLodThreshold>> Thresholds;
    SlLib::Math::Matrix4x4 ObbTransform{};
    SlLib::Math::Vector4 Extents{};
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuBranch
{
    std::int16_t Parent = -1;
    std::int16_t Child = -1;
    std::int16_t Sibling = -1;
    std::int16_t Flags = 0;
    std::string Name;
    std::shared_ptr<SuBlindData> BlindData;
    std::shared_ptr<SuLodBranch> Lod;
    std::shared_ptr<SuRenderMesh> Mesh;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuCollisionTriangle
{
    SlLib::Math::Vector4 A{};
    SlLib::Math::Vector4 B{};
    SlLib::Math::Vector4 C{};
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuCollisionMesh
{
    SlLib::Math::Matrix4x4 ObbTransform{};
    SlLib::Math::Vector4 Extents{};
    SlLib::Math::Vector4 Sphere{};
    int Hash = 0;
    std::int16_t Type = 0;
    std::int16_t BranchIndex = -1;
    std::shared_ptr<SuBlindData> BlindData;
    std::vector<std::shared_ptr<SuCollisionTriangle>> Triangles;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuLightData
{
    int Type = 0;
    float InnerConeAngle = 0.0f;
    float OuterConeAngle = 0.0f;
    int Falloff = 0;
    SlLib::Math::Vector4 ShadowColor{};
    bool IsShadowLight = false;
    int BranchIndex = 0;
    std::shared_ptr<SuRenderTexture> Texture;
    std::array<int, 5> AnimatedData{};
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuCameraData
{
    int Branch = 0;
    int Type = 0;
    float Fov = 0.0f;
    std::array<int, 2> AnimatedData{};
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuField
{
    int BranchIndex = 0;
    int Type = 0;
    float Magnitude = 0.0f;
    float Attenuation = 0.0f;
    SlLib::Math::Vector4 Direction{};
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRamp
{
    int Shift = 0;
    std::vector<int> Values;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuCurve
{
    int BranchId = 0;
    int Degree = 0;
    std::vector<SlLib::Math::Vector4> ControlPoints;
    std::vector<float> Knots;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuEmitterData
{
    int BranchIndex = 0;
    int Type = 0;
    float SpeedMin = 0.0f;
    float SpeedMax = 0.0f;
    float LifeMin = 0.0f;
    float LifeMax = 0.0f;
    int MaxCount = 0;
    float Spread = 0.0f;
    SlLib::Math::Vector4 Direction{};
    float RandomDirection = 0.0f;
    float DirectionalSpeed = 0.0f;
    int FaceVelocity = 0;
    float InheritFactor = 0.0f;
    float ConserveFactor = 0.0f;
    float ConvertorFactor = 0.0f;
    int ParticleBranch = 0;
    float Curviness = 0.0f;
    int UseFog = 0;
    int Refraction = 0;
    int VolumeShape = 0;
    float VolumeSweep = 0.0f;
    float AwayFromCenter = 0.0f;
    float AlongAxis = 0.0f;
    float AroundAxis = 0.0f;
    float AwayFromAxis = 0.0f;
    float TangentSpeed = 0.0f;
    float NormalSpeed = 0.0f;
    float Rotation = 0.0f;
    float RotateSpeed = 0.0f;
    int Color = 0;
    int RandomColor = 0;
    float Width = 0.0f;
    float Height = 0.0f;
    float RandomScale = 0.0f;
    float ScrollSpeedU = 0.0f;
    float ScrollSpeedV = 0.0f;
    int Layer = 0;
    int PlaneMode = 0;
    float PlaneConst = 0.0f;
    SlLib::Math::Vector4 PlaneNormal{};
    int Flags = 0;

    std::vector<std::shared_ptr<SuField>> Fields;
    std::shared_ptr<SuRamp> ColorRamp;
    std::shared_ptr<SuRamp> WidthRamp;
    std::shared_ptr<SuRamp> HeightRamp;
    std::shared_ptr<SuCurve> Curve;
    std::shared_ptr<SuRenderMaterial> Material;
    std::shared_ptr<SuRenderTexture> Texture;

    std::array<int, 2> AnimatedData{};
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuTextureTransform
{
    float PosU = 0.0f;
    float PosV = 0.0f;
    float Angle = 0.0f;
    float ScaleU = 1.0f;
    float ScaleV = 1.0f;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct StreamOverride
{
    int Hash = 0;
    int Index = 0;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuAnimationSample
{
    SlLib::Math::Vector4 Translation{};
    SlLib::Math::Vector4 Rotation{};
    SlLib::Math::Vector4 Scale{1.0f, 1.0f, 1.0f, 1.0f};
    bool Visible = true;
};

struct SuAnimationQuantRange
{
    float Minimum = 0.0f;
    float Delta = 0.0f;
    bool Valid = false;
};

struct SuAnimationQuantization
{
    std::vector<std::array<SuAnimationQuantRange, 3>> Translation;
    std::vector<std::array<SuAnimationQuantRange, 4>> Rotation;
    std::vector<std::array<SuAnimationQuantRange, 3>> Scale;
    std::vector<SuAnimationQuantRange> Visibility;

    void Resize(std::size_t bones);
};

struct SuAnimation
{
    int Type = 0;
    int NumFrames = 0;
    int NumBones = 0;
    int NumUvBones = 0;
    int NumFloatStreams = 0;

    std::vector<int> ChannelMasks;
    std::vector<std::int16_t> AnimStreams;
    std::vector<int> VectorOffsets;
    std::vector<SlLib::Math::Vector4> VectorFrameData;
    std::vector<float> FloatFrameData;
    std::vector<std::int16_t> FrameData;
    std::vector<std::uint8_t> Type6Block;
    bool Type6BigEndian = false;
    int Type6MaskEntrySize = 4;
    const std::uint8_t* Type6DataPtr = nullptr;
    std::size_t Type6DataSize = 0;
    const std::uint8_t* Type6GpuDataPtr = nullptr;
    std::size_t Type6GpuDataSize = 0;
    bool Type6ParamDataIsGpu = false;
    int Type6ParamDataOffset = 0;
    std::size_t Type6Anchor = 0;
    std::size_t Type6BlockStart = 0;
    std::size_t Type6BlockEnd = 0;
    std::vector<std::uint8_t> Type6DebugWindow;
    std::size_t Type6DebugWindowOffset = 0;
    bool Type6DebugMasksLogged = false;
    int Type6MaskNonZero = 0;
    std::array<std::uint32_t, 4> Type6MaskSample{};

    std::vector<SuAnimationSample> Samples;
    SuAnimationQuantization Quantization;
    bool SamplesDecoded = false;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
    bool DecodeType6Samples(SuRenderTree const& tree);
    SuAnimationSample const* GetSample(int frame, int bone) const;
};

struct SuAnimationEntry
{
    std::shared_ptr<SuAnimation> Animation;
    std::string AnimName;
    int Hash = 0;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuTreeGroup
{
    int Hash = 0;
    std::vector<int> TreeHashes;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRenderTree
{
    std::shared_ptr<SuBlindData> BlindData;
    int Hash = 0;
    std::vector<std::shared_ptr<SuBranch>> Branches;
    std::vector<SlLib::Math::Vector4> Translations;
    std::vector<SlLib::Math::Vector4> Rotations;
    std::vector<SlLib::Math::Vector4> Scales;
    std::vector<std::shared_ptr<SuCollisionMesh>> CollisionMeshes;
    std::vector<std::shared_ptr<SuLightData>> Lights;
    std::vector<std::shared_ptr<SuCameraData>> Cameras;
    std::vector<std::shared_ptr<SuEmitterData>> Emitters;
    std::vector<std::shared_ptr<SuCurve>> Curves;
    std::vector<SuTextureTransform> DefaultTextureTransforms;
    std::vector<SuAnimationEntry> AnimationEntries;
    std::vector<float> DefaultAnimationFloats;
    std::vector<StreamOverride> StreamOverrides;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct SuRenderForest
{
    std::string Name;
    std::vector<std::shared_ptr<SuRenderTree>> Trees;
    std::vector<std::shared_ptr<SuRenderTextureResource>> TextureResources;
    std::vector<std::shared_ptr<SuTreeGroup>> Groups;
    std::vector<std::shared_ptr<SuRenderTexture>> Textures;
    std::shared_ptr<SuBlindData> BlindData;

    void Load(SlLib::Serialization::ResourceLoadContext& context);
    int GetSizeForSerialization() const;
};

struct ForestLibrary
{
    struct ForestEntry
    {
        int Hash = 0;
        std::string Name;
        std::shared_ptr<SuRenderForest> Forest;
    };

    std::vector<ForestEntry> Forests;
    void Load(SlLib::Serialization::ResourceLoadContext& context);
};

} // namespace SeEditor::Forest
