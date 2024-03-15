void setupMaterial_d183faa1b8cb18d7(
	Material_d183faa1b8cb18d7 material,
	out vec3 base_color,
	out vec3 specular_color,
	out float specular_roughness,
	out float specular_IOR,
	out float coat,
	out float coat_roughness,
	out vec3 emission_color)
{
	// Graph input SS_ShaderRef1/base_color
	//{
vec3 SS_ShaderRef1_base_color//};
 = material.base_color;
	base_color = SS_ShaderRef1_base_color;// Output connection
	// Graph input SS_ShaderRef1/specular_color
	//{
vec3 SS_ShaderRef1_specular_color//};
 = material.specular_color;
	specular_color = SS_ShaderRef1_specular_color;// Output connection
	// Graph input SS_ShaderRef1/specular_roughness
	//{
float SS_ShaderRef1_specular_roughness//};
 = material.specular_roughness;
	specular_roughness = SS_ShaderRef1_specular_roughness;// Output connection
	// Graph input SS_ShaderRef1/specular_IOR
	//{
float SS_ShaderRef1_specular_IOR//};
 = material.specular_IOR;
	specular_IOR = SS_ShaderRef1_specular_IOR;// Output connection
	// Graph input SS_ShaderRef1/coat
	//{
float SS_ShaderRef1_coat//};
 = material.coat;
	coat = SS_ShaderRef1_coat;// Output connection
	// Graph input SS_ShaderRef1/coat_roughness
	//{
float SS_ShaderRef1_coat_roughness//};
 = material.coat_roughness;
	coat_roughness = SS_ShaderRef1_coat_roughness;// Output connection
	// Graph input SS_ShaderRef1/emission_color
	//{
vec3 SS_ShaderRef1_emission_color//};
 = material.emission_color;
	emission_color = SS_ShaderRef1_emission_color;// Output connection
}
