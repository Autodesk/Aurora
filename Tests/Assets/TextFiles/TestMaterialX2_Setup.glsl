void setupMaterial_498292f537214e2a(
	Material_498292f537214e2a material,
	sampler2D base_color_image_image_parameter,
	sampler2D specular_roughness_image_image_parameter,
	out vec3 base_color,
	out float metalness,
	out float specular_roughness,
	out float specular_IOR,
	out float transmission,
	out float coat,
	out float coat_roughness,
	out vec3 emission_color)
{
	//Temp input variables for base_color_image 
	vec3 nodeOutTmp_base_color_image_out; //Temp output variable for out 
	sampler2D nodeTmp_base_color_image_file; //Temp input variable for file 
	int nodeTmp_base_color_image_layer; //Temp input variable for layer 
	vec3 nodeTmp_base_color_image_default; //Temp input variable for default 
	vec2 nodeTmp_base_color_image_texcoord; //Temp input variable for texcoord 
	int nodeTmp_base_color_image_uaddressmode; //Temp input variable for uaddressmode 
	int nodeTmp_base_color_image_vaddressmode; //Temp input variable for vaddressmode 
	int nodeTmp_base_color_image_filtertype; //Temp input variable for filtertype 
	int nodeTmp_base_color_image_framerange; //Temp input variable for framerange 
	int nodeTmp_base_color_image_frameoffset; //Temp input variable for frameoffset 
	int nodeTmp_base_color_image_frameendaction; //Temp input variable for frameendaction 
	vec2 nodeTmp_base_color_image_uv_scale; //Temp input variable for uv_scale 
	vec2 nodeTmp_base_color_image_uv_offset; //Temp input variable for uv_offset 
	// Graph input NG1/base_color_image/file
	//{
sampler2D base_color_image_file1//};
 = base_color_image_image_parameter;
	nodeTmp_base_color_image_file = base_color_image_file1;// Output connection
	// Graph input NG1/base_color_image/layer
	//{
int base_color_image_layer1 = 0//};
;
	nodeTmp_base_color_image_layer = base_color_image_layer1;// Output connection
	// Graph input NG1/base_color_image/default
	//{
vec3 base_color_image_default1 = vec3(0, 0, 0)//};
;
	nodeTmp_base_color_image_default = base_color_image_default1;// Output connection
	//Temp input variables for geomprop_UV0 
	vec2 nodeOutTmp_geomprop_UV0_out; //Temp output variable for out 
	int nodeTmp_geomprop_UV0_index; //Temp input variable for index 
	// Graph input function call texcoord (See definition IM_texcoord_vector2_genglsl)
{
	//{
    vec2 geomprop_UV0_out1 = vertexData.texCoord;
//};
	nodeOutTmp_geomprop_UV0_out = geomprop_UV0_out1;// Output connection
	nodeTmp_base_color_image_texcoord = geomprop_UV0_out1;// Output connection
}
	// Graph input NG1/base_color_image/uaddressmode
	//{
int base_color_image_uaddressmode1 = 2//};
;
	nodeTmp_base_color_image_uaddressmode = base_color_image_uaddressmode1;// Output connection
	// Graph input NG1/base_color_image/vaddressmode
	//{
int base_color_image_vaddressmode1 = 2//};
;
	nodeTmp_base_color_image_vaddressmode = base_color_image_vaddressmode1;// Output connection
	// Graph input NG1/base_color_image/filtertype
	//{
int base_color_image_filtertype1 = 1//};
;
	nodeTmp_base_color_image_filtertype = base_color_image_filtertype1;// Output connection
	// Graph input NG1/base_color_image/framerange
	//{
int base_color_image_framerange1 = 0//};
;
	nodeTmp_base_color_image_framerange = base_color_image_framerange1;// Output connection
	// Graph input NG1/base_color_image/frameoffset
	//{
int base_color_image_frameoffset1 = 0//};
;
	nodeTmp_base_color_image_frameoffset = base_color_image_frameoffset1;// Output connection
	// Graph input NG1/base_color_image/frameendaction
	//{
int base_color_image_frameendaction1 = 0//};
;
	nodeTmp_base_color_image_frameendaction = base_color_image_frameendaction1;// Output connection
	// Graph input 
	//{
vec2 base_color_image_uv_scale1 = vec2(1, 1)//};
;
	nodeTmp_base_color_image_uv_scale = base_color_image_uv_scale1;// Output connection
	// Graph input 
	//{
vec2 base_color_image_uv_offset1 = vec2(0, 0)//};
;
	nodeTmp_base_color_image_uv_offset = base_color_image_uv_offset1;// Output connection
	// Graph input function call base_color (See definition IM_image_color3_genglsl)
{
	//{
    vec3 base_color_image_out = vec3(0.0);
    mx_image_color3(nodeTmp_base_color_image_file, nodeTmp_base_color_image_layer, nodeTmp_base_color_image_default, nodeTmp_base_color_image_texcoord, nodeTmp_base_color_image_uaddressmode, nodeTmp_base_color_image_vaddressmode, nodeTmp_base_color_image_filtertype, nodeTmp_base_color_image_framerange, nodeTmp_base_color_image_frameoffset, nodeTmp_base_color_image_frameendaction, nodeTmp_base_color_image_uv_scale, nodeTmp_base_color_image_uv_offset, base_color_image_out);
//};
	nodeOutTmp_base_color_image_out = base_color_image_out;// Output connection
	base_color = base_color_image_out;// Output connection
}
	// Graph input SS_Material/metalness
	//{
float SS_Material_metalness//};
 = material.metalness;
	metalness = SS_Material_metalness;// Output connection
	//Temp input variables for specular_roughness_image 
	float nodeOutTmp_specular_roughness_image_out; //Temp output variable for out 
	sampler2D nodeTmp_specular_roughness_image_file; //Temp input variable for file 
	int nodeTmp_specular_roughness_image_layer; //Temp input variable for layer 
	float nodeTmp_specular_roughness_image_default; //Temp input variable for default 
	vec2 nodeTmp_specular_roughness_image_texcoord; //Temp input variable for texcoord 
	int nodeTmp_specular_roughness_image_uaddressmode; //Temp input variable for uaddressmode 
	int nodeTmp_specular_roughness_image_vaddressmode; //Temp input variable for vaddressmode 
	int nodeTmp_specular_roughness_image_filtertype; //Temp input variable for filtertype 
	int nodeTmp_specular_roughness_image_framerange; //Temp input variable for framerange 
	int nodeTmp_specular_roughness_image_frameoffset; //Temp input variable for frameoffset 
	int nodeTmp_specular_roughness_image_frameendaction; //Temp input variable for frameendaction 
	vec2 nodeTmp_specular_roughness_image_uv_scale; //Temp input variable for uv_scale 
	vec2 nodeTmp_specular_roughness_image_uv_offset; //Temp input variable for uv_offset 
	// Graph input NG2/specular_roughness_image/file
	//{
sampler2D specular_roughness_image_file1//};
 = specular_roughness_image_image_parameter;
	nodeTmp_specular_roughness_image_file = specular_roughness_image_file1;// Output connection
	// Graph input NG2/specular_roughness_image/layer
	//{
int specular_roughness_image_layer1 = 0//};
;
	nodeTmp_specular_roughness_image_layer = specular_roughness_image_layer1;// Output connection
	// Graph input NG2/specular_roughness_image/default
	//{
float specular_roughness_image_default1 = 0//};
;
	nodeTmp_specular_roughness_image_default = specular_roughness_image_default1;// Output connection
	nodeTmp_specular_roughness_image_texcoord = nodeOutTmp_geomprop_UV0_out;// Output connection
	// Graph input NG2/specular_roughness_image/uaddressmode
	//{
int specular_roughness_image_uaddressmode1 = 2//};
;
	nodeTmp_specular_roughness_image_uaddressmode = specular_roughness_image_uaddressmode1;// Output connection
	// Graph input NG2/specular_roughness_image/vaddressmode
	//{
int specular_roughness_image_vaddressmode1 = 2//};
;
	nodeTmp_specular_roughness_image_vaddressmode = specular_roughness_image_vaddressmode1;// Output connection
	// Graph input NG2/specular_roughness_image/filtertype
	//{
int specular_roughness_image_filtertype1 = 1//};
;
	nodeTmp_specular_roughness_image_filtertype = specular_roughness_image_filtertype1;// Output connection
	// Graph input NG2/specular_roughness_image/framerange
	//{
int specular_roughness_image_framerange1 = 0//};
;
	nodeTmp_specular_roughness_image_framerange = specular_roughness_image_framerange1;// Output connection
	// Graph input NG2/specular_roughness_image/frameoffset
	//{
int specular_roughness_image_frameoffset1 = 0//};
;
	nodeTmp_specular_roughness_image_frameoffset = specular_roughness_image_frameoffset1;// Output connection
	// Graph input NG2/specular_roughness_image/frameendaction
	//{
int specular_roughness_image_frameendaction1 = 0//};
;
	nodeTmp_specular_roughness_image_frameendaction = specular_roughness_image_frameendaction1;// Output connection
	// Graph input 
	//{
vec2 specular_roughness_image_uv_scale1 = vec2(1, 1)//};
;
	nodeTmp_specular_roughness_image_uv_scale = specular_roughness_image_uv_scale1;// Output connection
	// Graph input 
	//{
vec2 specular_roughness_image_uv_offset1 = vec2(0, 0)//};
;
	nodeTmp_specular_roughness_image_uv_offset = specular_roughness_image_uv_offset1;// Output connection
	// Graph input function call specular_roughness (See definition IM_image_float_genglsl)
{
	//{
    float specular_roughness_image_out = 0.0;
    mx_image_float(nodeTmp_specular_roughness_image_file, nodeTmp_specular_roughness_image_layer, nodeTmp_specular_roughness_image_default, nodeTmp_specular_roughness_image_texcoord, nodeTmp_specular_roughness_image_uaddressmode, nodeTmp_specular_roughness_image_vaddressmode, nodeTmp_specular_roughness_image_filtertype, nodeTmp_specular_roughness_image_framerange, nodeTmp_specular_roughness_image_frameoffset, nodeTmp_specular_roughness_image_frameendaction, nodeTmp_specular_roughness_image_uv_scale, nodeTmp_specular_roughness_image_uv_offset, specular_roughness_image_out);
//};
	nodeOutTmp_specular_roughness_image_out = specular_roughness_image_out;// Output connection
	specular_roughness = specular_roughness_image_out;// Output connection
}
	// Graph input SS_Material/specular_IOR
	//{
float SS_Material_specular_IOR//};
 = material.specular_IOR;
	specular_IOR = SS_Material_specular_IOR;// Output connection
	// Graph input SS_Material/transmission
	//{
float SS_Material_transmission//};
 = material.transmission;
	transmission = SS_Material_transmission;// Output connection
	// Graph input SS_Material/coat
	//{
float SS_Material_coat//};
 = material.coat;
	coat = SS_Material_coat;// Output connection
	// Graph input SS_Material/coat_roughness
	//{
float SS_Material_coat_roughness//};
 = material.coat_roughness;
	coat_roughness = SS_Material_coat_roughness;// Output connection
	// Graph input SS_Material/emission_color
	//{
vec3 SS_Material_emission_color//};
 = material.emission_color;
	emission_color = SS_Material_emission_color;// Output connection
}
