// Auto-generated by unit test BasicTest in AuroraExternals test suite.

struct MaterialConstants
{
	float base; // Offset:0 Property:base
	float3 baseColor; // Offset:4 Property:base_color
	float diffuseRoughness; // Offset:16 Property:diffuse_roughness
	float metalness; // Offset:20 Property:metalness
	float specular; // Offset:24 Property:specular
	int _padding0;
	float3 specularColor; // Offset:32 Property:specular_color
	float specularRoughness; // Offset:44 Property:specular_roughness
	float specularIOR; // Offset:48 Property:specular_IOR
	float specularAnisotropy; // Offset:52 Property:specular_anisotropy
	float specularRotation; // Offset:56 Property:specular_rotation
	float transmission; // Offset:60 Property:transmission
	float3 transmissionColor; // Offset:64 Property:transmission_color
	float subsurface; // Offset:76 Property:subsurface
	float3 subsurfaceColor; // Offset:80 Property:subsurface_color
	int _padding1;
	float3 subsurfaceRadius; // Offset:96 Property:subsurface_radius
	float subsurfaceScale; // Offset:108 Property:subsurface_scale
	float subsurfaceAnisotropy; // Offset:112 Property:subsurface_anisotropy
	float sheen; // Offset:116 Property:sheen
	int _padding2;
	int _padding3;
	float3 sheenColor; // Offset:128 Property:sheen_color
	float sheenRoughness; // Offset:140 Property:sheen_roughness
	float coat; // Offset:144 Property:coat
	float3 coatColor; // Offset:148 Property:coat_color
	float coatRoughness; // Offset:160 Property:coat_roughness
	float coatAnisotropy; // Offset:164 Property:coat_anisotropy
	float coatRotation; // Offset:168 Property:coat_rotation
	float coatIOR; // Offset:172 Property:coat_IOR
	float coatAffectColor; // Offset:176 Property:coat_affect_color
	float coatAffectRoughness; // Offset:180 Property:coat_affect_roughness
	float emission; // Offset:184 Property:emission
	int _padding4;
	float3 emissionColor; // Offset:192 Property:emission_color
	int _padding5;
	float3 opacity; // Offset:208 Property:opacity
	int thinWalled; // Offset:220 Property:thin_walled
	int hasBaseColorTex; // Offset:224 Property:has_base_color_image
	float2 baseColorTexOffset; // Offset:228 Property:base_color_image_offset
	int _padding6;
	float2 baseColorTexScale; // Offset:240 Property:base_color_image_scale
	float2 baseColorTexPivot; // Offset:248 Property:base_color_image_pivot
	float baseColorTexRotation; // Offset:256 Property:base_color_image_rotation
	int hasSpecularRoughnessTex; // Offset:260 Property:has_specular_roughness_image
	float2 specularRoughnessTexOffset; // Offset:264 Property:specular_roughness_image_offset
	float2 specularRoughnessTexScale; // Offset:272 Property:specular_roughness_image_scale
	float2 specularRoughnessTexPivot; // Offset:280 Property:specular_roughness_image_pivot
	float specularRoughnessTexRotation; // Offset:288 Property:specular_roughness_image_rotation
	int hasEmissionColorTex; // Offset:292 Property:has_emission_color_image
	float2 emissionColorTexOffset; // Offset:296 Property:emission_color_image_offset
	float2 emissionColorTexScale; // Offset:304 Property:emission_color_image_scale
	float2 emissionColorTexPivot; // Offset:312 Property:emission_color_image_pivot
	float emissionColorTexRotation; // Offset:320 Property:emission_color_image_rotation
	int hasOpacityTex; // Offset:324 Property:has_opacity_image
	float2 opacityTexOffset; // Offset:328 Property:opacity_image_offset
	float2 opacityTexScale; // Offset:336 Property:opacity_image_scale
	float2 opacityTexPivot; // Offset:344 Property:opacity_image_pivot
	float opacityTexRotation; // Offset:352 Property:opacity_image_rotation
	int hasNormalTex; // Offset:356 Property:has_normal_image
	float2 normalTexOffset; // Offset:360 Property:normal_image_offset
	float2 normalTexScale; // Offset:368 Property:normal_image_scale
	float2 normalTexPivot; // Offset:376 Property:normal_image_pivot
	float normalTexRotation; // Offset:384 Property:normal_image_rotation
	int _padding7;
	int _padding8;
	int _padding9;
}
;

float Material_base(MaterialConstants mtl) {
	return mtl.base;
}

float3 Material_baseColor(MaterialConstants mtl) {
	return mtl.baseColor;
}

float Material_diffuseRoughness(MaterialConstants mtl) {
	return mtl.diffuseRoughness;
}

float Material_metalness(MaterialConstants mtl) {
	return mtl.metalness;
}

float Material_specular(MaterialConstants mtl) {
	return mtl.specular;
}

float3 Material_specularColor(MaterialConstants mtl) {
	return mtl.specularColor;
}

float Material_specularRoughness(MaterialConstants mtl) {
	return mtl.specularRoughness;
}

float Material_specularIOR(MaterialConstants mtl) {
	return mtl.specularIOR;
}

float Material_specularAnisotropy(MaterialConstants mtl) {
	return mtl.specularAnisotropy;
}

float Material_specularRotation(MaterialConstants mtl) {
	return mtl.specularRotation;
}

float Material_transmission(MaterialConstants mtl) {
	return mtl.transmission;
}

float3 Material_transmissionColor(MaterialConstants mtl) {
	return mtl.transmissionColor;
}

float Material_subsurface(MaterialConstants mtl) {
	return mtl.subsurface;
}

float3 Material_subsurfaceColor(MaterialConstants mtl) {
	return mtl.subsurfaceColor;
}

float3 Material_subsurfaceRadius(MaterialConstants mtl) {
	return mtl.subsurfaceRadius;
}

float Material_subsurfaceScale(MaterialConstants mtl) {
	return mtl.subsurfaceScale;
}

float Material_subsurfaceAnisotropy(MaterialConstants mtl) {
	return mtl.subsurfaceAnisotropy;
}

float Material_sheen(MaterialConstants mtl) {
	return mtl.sheen;
}

float3 Material_sheenColor(MaterialConstants mtl) {
	return mtl.sheenColor;
}

float Material_sheenRoughness(MaterialConstants mtl) {
	return mtl.sheenRoughness;
}

float Material_coat(MaterialConstants mtl) {
	return mtl.coat;
}

float3 Material_coatColor(MaterialConstants mtl) {
	return mtl.coatColor;
}

float Material_coatRoughness(MaterialConstants mtl) {
	return mtl.coatRoughness;
}

float Material_coatAnisotropy(MaterialConstants mtl) {
	return mtl.coatAnisotropy;
}

float Material_coatRotation(MaterialConstants mtl) {
	return mtl.coatRotation;
}

float Material_coatIOR(MaterialConstants mtl) {
	return mtl.coatIOR;
}

float Material_coatAffectColor(MaterialConstants mtl) {
	return mtl.coatAffectColor;
}

float Material_coatAffectRoughness(MaterialConstants mtl) {
	return mtl.coatAffectRoughness;
}

float Material_emission(MaterialConstants mtl) {
	return mtl.emission;
}

float3 Material_emissionColor(MaterialConstants mtl) {
	return mtl.emissionColor;
}

float3 Material_opacity(MaterialConstants mtl) {
	return mtl.opacity;
}

int Material_thinWalled(MaterialConstants mtl) {
	return mtl.thinWalled;
}

int Material_hasBaseColorTex(MaterialConstants mtl) {
	return mtl.hasBaseColorTex;
}

float2 Material_baseColorTexOffset(MaterialConstants mtl) {
	return mtl.baseColorTexOffset;
}

float2 Material_baseColorTexScale(MaterialConstants mtl) {
	return mtl.baseColorTexScale;
}

float2 Material_baseColorTexPivot(MaterialConstants mtl) {
	return mtl.baseColorTexPivot;
}

float Material_baseColorTexRotation(MaterialConstants mtl) {
	return mtl.baseColorTexRotation;
}

int Material_hasSpecularRoughnessTex(MaterialConstants mtl) {
	return mtl.hasSpecularRoughnessTex;
}

float2 Material_specularRoughnessTexOffset(MaterialConstants mtl) {
	return mtl.specularRoughnessTexOffset;
}

float2 Material_specularRoughnessTexScale(MaterialConstants mtl) {
	return mtl.specularRoughnessTexScale;
}

float2 Material_specularRoughnessTexPivot(MaterialConstants mtl) {
	return mtl.specularRoughnessTexPivot;
}

float Material_specularRoughnessTexRotation(MaterialConstants mtl) {
	return mtl.specularRoughnessTexRotation;
}

int Material_hasEmissionColorTex(MaterialConstants mtl) {
	return mtl.hasEmissionColorTex;
}

float2 Material_emissionColorTexOffset(MaterialConstants mtl) {
	return mtl.emissionColorTexOffset;
}

float2 Material_emissionColorTexScale(MaterialConstants mtl) {
	return mtl.emissionColorTexScale;
}

float2 Material_emissionColorTexPivot(MaterialConstants mtl) {
	return mtl.emissionColorTexPivot;
}

float Material_emissionColorTexRotation(MaterialConstants mtl) {
	return mtl.emissionColorTexRotation;
}

int Material_hasOpacityTex(MaterialConstants mtl) {
	return mtl.hasOpacityTex;
}

float2 Material_opacityTexOffset(MaterialConstants mtl) {
	return mtl.opacityTexOffset;
}

float2 Material_opacityTexScale(MaterialConstants mtl) {
	return mtl.opacityTexScale;
}

float2 Material_opacityTexPivot(MaterialConstants mtl) {
	return mtl.opacityTexPivot;
}

float Material_opacityTexRotation(MaterialConstants mtl) {
	return mtl.opacityTexRotation;
}

int Material_hasNormalTex(MaterialConstants mtl) {
	return mtl.hasNormalTex;
}

float2 Material_normalTexOffset(MaterialConstants mtl) {
	return mtl.normalTexOffset;
}

float2 Material_normalTexScale(MaterialConstants mtl) {
	return mtl.normalTexScale;
}

float2 Material_normalTexPivot(MaterialConstants mtl) {
	return mtl.normalTexPivot;
}

float Material_normalTexRotation(MaterialConstants mtl) {
	return mtl.normalTexRotation;
}
